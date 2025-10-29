#include "external_argument_completion.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "cjsh_filesystem.h"
#include "completion_tracker.h"
#include "completion_utils.h"

namespace external_argument_completion {
namespace {

struct CompletionEntry {
    std::string value;
    std::string description;
};

struct CommandCompletionData {
    std::string command;
    std::string summary;
    std::vector<CompletionEntry> subcommands;
    std::vector<CompletionEntry> flags;
    std::filesystem::path file_path;
};

std::string ltrim_copy(const std::string& input) {
    size_t pos = 0;
    while (pos < input.size() && std::isspace(static_cast<unsigned char>(input[pos]))) {
        ++pos;
    }
    return input.substr(pos);
}

std::string rtrim_copy(const std::string& input) {
    if (input.empty()) {
        return input;
    }
    size_t end = input.size();
    while (end > 0 && std::isspace(static_cast<unsigned char>(input[end - 1]))) {
        --end;
    }
    return input.substr(0, end);
}

std::string trim_copy(const std::string& input) {
    return rtrim_copy(ltrim_copy(input));
}

bool is_all_upper(const std::string& text) {
    bool has_alpha = false;
    for (char ch : text) {
        if (std::isalpha(static_cast<unsigned char>(ch))) {
            has_alpha = true;
            if (std::tolower(static_cast<unsigned char>(ch)) != ch) {
                continue;
            }
            if (std::toupper(static_cast<unsigned char>(ch)) != ch) {
                return false;
            }
        }
    }
    return has_alpha;
}

std::string to_upper_copy(const std::string& input) {
    std::string result = input;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return result;
}

bool is_section_heading(const std::string& line) {
    std::string trimmed = trim_copy(line);
    if (trimmed.empty()) {
        return false;
    }
    if (trimmed.size() > 60) {
        return false;
    }
    return is_all_upper(trimmed);
}

std::string strip_overstrike_sequences(const std::string& text) {
    std::string result;
    result.reserve(text.size());
    for (char ch : text) {
        if (ch == '\b') {
            if (!result.empty()) {
                result.pop_back();
            }
            continue;
        }
        result.push_back(ch);
    }
    return result;
}

bool is_supported_command_name(const std::string& name) {
    if (name.empty()) {
        return false;
    }
    for (char ch : name) {
        if (std::isalnum(static_cast<unsigned char>(ch)) || ch == '-' || ch == '_' || ch == '.') {
            continue;
        }
        return false;
    }
    return true;
}

std::string sanitize_for_filename(const std::string& command) {
    std::string result;
    result.reserve(command.size());
    for (char ch : command) {
        if (std::isalnum(static_cast<unsigned char>(ch)) || ch == '-' || ch == '_') {
            result.push_back(ch);
        } else if (ch == '.') {
            result.push_back('_');
        } else {
            result.push_back('_');
        }
    }
    if (result.empty()) {
        result = "cmd";
    }
    if (result != command) {
        std::ostringstream oss;
        oss << '_' << std::hex << std::hash<std::string>{}(command);
        result += oss.str();
    }
    return result;
}

std::string truncate_description(const std::string& description) {
    constexpr size_t kMaxLen = 120;
    if (description.size() <= kMaxLen) {
        return description;
    }
    return description.substr(0, kMaxLen - 3) + "...";
}

constexpr auto kNoCompletionRetryDelay = std::chrono::hours(12);
constexpr unsigned int kMinWorkerThreads = 2;
constexpr unsigned int kMaxWorkerThreads = 16;

bool looks_like_subcommand_name(const std::string& token) {
    if (token.empty()) {
        return false;
    }
    unsigned char first = static_cast<unsigned char>(token.front());
    if (std::islower(first) || std::isdigit(first)) {
        return true;
    }
    if (first == '_' || first == '-') {
        return true;
    }
    return false;
}

std::string sanitize_subcommand_token(std::string token, const std::string& parent_command) {
    token = trim_copy(token);
    if (token.empty()) {
        return token;
    }

    while (!token.empty() && (token.back() == ':' || token.back() == ';' || token.back() == '.' ||
                              token.back() == ',')) {
        token.pop_back();
    }

    size_t paren_pos = token.find('(');
    if (paren_pos != std::string::npos && token.back() == ')') {
        bool digits_only = true;
        for (size_t i = paren_pos + 1; i + 1 < token.size(); ++i) {
            if (!std::isdigit(static_cast<unsigned char>(token[i]))) {
                digits_only = false;
                break;
            }
        }
        if (digits_only) {
            token = token.substr(0, paren_pos);
        }
    }

    if (!parent_command.empty()) {
        std::string prefix = parent_command + "-";
        if (token.rfind(prefix, 0) == 0 && token.size() > prefix.size()) {
            token = token.substr(prefix.size());
        }
    }

    return trim_copy(token);
}

struct OptionLine {
    std::vector<std::string> tokens;
    std::string description;
    bool valid = false;
};

OptionLine parse_option_line(const std::string& line) {
    OptionLine option;

    size_t pos = line.find_first_not_of(' ');
    if (pos == std::string::npos || line[pos] != '-') {
        return option;
    }

    size_t desc_start = line.find("  ", pos);
    std::string token_part;
    if (desc_start == std::string::npos) {
        token_part = line.substr(pos);
    } else {
        token_part = line.substr(pos, desc_start - pos);
    }
    std::replace(token_part.begin(), token_part.end(), '|', ',');
    std::string desc_part;
    if (desc_start != std::string::npos) {
        desc_part = trim_copy(line.substr(desc_start));
    }

    std::stringstream token_stream(token_part);
    std::string token;
    while (std::getline(token_stream, token, ',')) {
        token = trim_copy(token);
        if (!token.empty()) {
            size_t space_pos = token.find_first_of(" \t");
            if (space_pos != std::string::npos) {
                token = token.substr(0, space_pos);
            }
            if (!token.empty()) {
                option.tokens.push_back(token);
            }
        }
    }

    if (option.tokens.empty()) {
        return option;
    }

    option.description = desc_part;
    option.valid = true;
    return option;
}

struct SubcommandLine {
    std::string token;
    std::string description;
    bool valid = false;
};

SubcommandLine parse_subcommand_line(const std::string& line, const std::string& parent_command) {
    SubcommandLine entry;

    size_t pos = line.find_first_not_of(' ');
    if (pos == std::string::npos) {
        return entry;
    }

    if (!std::isalnum(static_cast<unsigned char>(line[pos]))) {
        return entry;
    }

    size_t desc_start = line.find("  ", pos);
    std::string token_part;
    if (desc_start == std::string::npos) {
        token_part = trim_copy(line.substr(pos));
    } else {
        token_part = trim_copy(line.substr(pos, desc_start - pos));
    }
    if (token_part.empty()) {
        return entry;
    }
    if (token_part.find(' ') != std::string::npos || token_part.find('\t') != std::string::npos) {
        return entry;
    }

    std::string sanitized = sanitize_subcommand_token(token_part, parent_command);
    if (sanitized.empty() || !looks_like_subcommand_name(sanitized)) {
        return entry;
    }

    entry.token = std::move(sanitized);
    if (entry.token.empty()) {
        return entry;
    }

    if (desc_start != std::string::npos) {
        entry.description = trim_copy(line.substr(desc_start));
    }
    entry.valid = true;
    return entry;
}

std::string read_stream(FILE* pipe) {
    if (pipe == nullptr) {
        return {};
    }

    std::string output;
    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output.append(buffer);
        if (output.size() > 2 * 1024 * 1024) {
            break;
        }
    }
    return output;
}

std::optional<std::string> fetch_man_page(const std::string& command) {
    if (!is_supported_command_name(command)) {
        return std::nullopt;
    }

    std::string cmd = "MANPAGER=cat MANWIDTH=1000 man " + command + " 2>/dev/null";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        return std::nullopt;
    }

    std::string output = read_stream(pipe);
    pclose(pipe);

    if (output.empty()) {
        return std::nullopt;
    }

    return strip_overstrike_sequences(output);
}

enum class Section : std::uint8_t {
    None,
    Name,
    Description,
    Options,
    Commands
};

Section map_section(const std::string& heading) {
    std::string upper = to_upper_copy(trim_copy(heading));
    if (upper == "NAME") {
        return Section::Name;
    }
    if (upper == "DESCRIPTION") {
        return Section::Description;
    }
    if (upper == "OPTIONS" || upper == "FLAGS" || upper.find("OPTIONS") != std::string::npos) {
        return Section::Options;
    }
    if (upper == "COMMANDS" || upper == "SUBCOMMANDS" || upper == "ACTIONS" ||
        upper.find(" COMMAND") != std::string::npos ||
        upper.find("COMMANDS") != std::string::npos) {
        return Section::Commands;
    }
    return Section::None;
}

enum class JobType : std::uint8_t {
    Command,
    Scan
};

struct Job {
    JobType type;
    std::string command;
};

class CompletionManager {
   public:
    CompletionManager();
    ~CompletionManager();

    void initialize();
    bool add_completions(ic_completion_env_t* cenv, const std::string& command,
                         const std::vector<std::string>& args, bool at_new_token);

   private:
    void ensure_directory();
    void load_existing_files();
    void load_completion_file(const std::filesystem::path& path);
    void load_no_completion_file(const std::filesystem::path& path);
    void scan_path_for_changes();
    void remove_missing_commands(const std::unordered_set<std::string>& commands_in_path);
    void request_generation(const std::string& command);
    void enqueue_command(const std::string& command);
    void enqueue_path_scan();
    void start_workers();
    unsigned int determine_worker_target() const;
    void worker_loop();
    void generate_for_command(const std::string& command);
    std::optional<CommandCompletionData> parse_man_page_for_command(const std::string& command);
    bool write_completion_file(const CommandCompletionData& data);
    void mark_no_completion(const std::string& command,
                            const std::chrono::system_clock::time_point& when);
    void clear_no_completion(const std::string& command);

    std::filesystem::path command_to_path(const std::string& command) const;
    std::filesystem::path command_to_negative_path(const std::string& command) const;

    std::once_flag init_flag_{};
    std::mutex mutex_;
    std::unordered_map<std::string, CommandCompletionData> cache_;
    std::unordered_map<std::string, std::chrono::system_clock::time_point> no_completion_commands_;
    std::unordered_set<std::string> persistent_negative_commands_;

    std::vector<std::thread> workers_;
    std::condition_variable cv_;
    std::mutex queue_mutex_;
    std::deque<Job> job_queue_;
    std::unordered_set<std::string> pending_commands_;
    bool scan_pending_ = false;
    bool stop_worker_ = false;
    bool workers_started_ = false;
    unsigned int worker_target_ = 0;
};

CompletionManager::CompletionManager() = default;

CompletionManager::~CompletionManager() {
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        stop_worker_ = true;
    }
    cv_.notify_all();
    if (workers_started_) {
        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }
}

void CompletionManager::initialize() {
    std::call_once(init_flag_, [this]() {
        ensure_directory();
        load_existing_files();
        auto executables = cjsh_filesystem::get_executables_in_path();
        std::unordered_set<std::string> executable_set(executables.begin(), executables.end());
        remove_missing_commands(executable_set);

        std::vector<std::string> missing;
        std::vector<std::string> expired_negatives;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto now = std::chrono::system_clock::now();
            for (const auto& cmd : executables) {
                if (cache_.find(cmd) != cache_.end()) {
                    continue;
                }
                auto neg_it = no_completion_commands_.find(cmd);
                if (neg_it != no_completion_commands_.end()) {
                    if (now - neg_it->second < kNoCompletionRetryDelay) {
                        continue;
                    }
                    expired_negatives.push_back(cmd);
                }
                missing.push_back(cmd);
            }
        }

        for (const auto& cmd : expired_negatives) {
            clear_no_completion(cmd);
        }

        if (!missing.empty()) {
            start_workers();
            for (const auto& cmd : missing) {
                enqueue_command(cmd);
            }
        }
    });
}

void CompletionManager::start_workers() {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    if (workers_started_) {
        return;
    }
    worker_target_ = determine_worker_target();
    worker_target_ = std::max(worker_target_, kMinWorkerThreads);
    worker_target_ = std::min(worker_target_, kMaxWorkerThreads);
    workers_started_ = true;
    workers_.reserve(worker_target_);
    for (unsigned int i = 0; i < worker_target_; ++i) {
        workers_.emplace_back([this]() { worker_loop(); });
    }
}

unsigned int CompletionManager::determine_worker_target() const {
    unsigned int default_workers = std::thread::hardware_concurrency();
    if (default_workers == 0) {
        default_workers = 2;
    }
    default_workers = std::max(default_workers, kMinWorkerThreads);
    default_workers = std::min(default_workers, kMaxWorkerThreads);

    const char* env = std::getenv("CJSH_COMPLETION_WORKERS");
    if (env != nullptr && *env != '\0') {
        char* end = nullptr;
        unsigned long parsed = std::strtoul(env, &end, 10);
        if (end != env && *end == '\0' && parsed > 0) {
            unsigned long clamped = std::min(parsed, static_cast<unsigned long>(kMaxWorkerThreads));
            clamped = std::max(clamped, static_cast<unsigned long>(kMinWorkerThreads));
            default_workers = static_cast<unsigned int>(clamped);
        }
    }

    return default_workers;
}

void CompletionManager::enqueue_path_scan() {
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        if (!workers_started_ || scan_pending_) {
            return;
        }
        job_queue_.push_back(Job{JobType::Scan, {}});
        scan_pending_ = true;
    }
    cv_.notify_one();
}

void CompletionManager::request_generation(const std::string& command) {
    if (command.empty()) {
        return;
    }

    bool should_clear_negative = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (cache_.find(command) != cache_.end()) {
            return;
        }
        auto now = std::chrono::system_clock::now();
        auto neg_it = no_completion_commands_.find(command);
        if (neg_it != no_completion_commands_.end()) {
            if (now - neg_it->second < kNoCompletionRetryDelay) {
                return;
            }
            should_clear_negative = true;
        } else if (persistent_negative_commands_.find(command) !=
                   persistent_negative_commands_.end()) {
            should_clear_negative = true;
        }
    }

    if (should_clear_negative) {
        clear_no_completion(command);
    }

    if (cjsh_filesystem::find_executable_in_path(command).empty()) {
        return;
    }

    start_workers();
    enqueue_command(command);
}

void CompletionManager::enqueue_command(const std::string& command) {
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        if (!workers_started_) {
            return;
        }
        if (!pending_commands_.insert(command).second) {
            return;
        }
        job_queue_.push_back(Job{JobType::Command, command});
    }
    cv_.notify_one();
}

void CompletionManager::worker_loop() {
    for (;;) {
        Job job{JobType::Command, {}};
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            cv_.wait(lock, [this]() { return stop_worker_ || !job_queue_.empty(); });
            if (stop_worker_ && job_queue_.empty()) {
                return;
            }
            job = std::move(job_queue_.front());
            job_queue_.pop_front();
            if (job.type == JobType::Scan) {
                scan_pending_ = false;
            }
        }

        if (job.type == JobType::Scan) {
            scan_path_for_changes();
        } else {
            generate_for_command(job.command);
            std::lock_guard<std::mutex> lock(queue_mutex_);
            pending_commands_.erase(job.command);
        }
    }
}

bool CompletionManager::add_completions(ic_completion_env_t* cenv, const std::string& command,
                                        const std::vector<std::string>& args, bool at_new_token) {
    CommandCompletionData data_copy;
    bool found = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = cache_.find(command);
        if (it != cache_.end()) {
            data_copy = it->second;
            found = true;
        } else {
            auto neg_it = no_completion_commands_.find(command);
            if (neg_it != no_completion_commands_.end()) {
                auto now = std::chrono::system_clock::now();
                if (now - neg_it->second < kNoCompletionRetryDelay) {
                    return false;
                }
                no_completion_commands_.erase(neg_it);
            }
        }
    }

    if (!found) {
        request_generation(command);
        return false;
    }

    if (data_copy.subcommands.empty() && data_copy.flags.empty()) {
        return false;
    }

    std::string current_token;
    if (!args.empty() && !at_new_token) {
        current_token = args.back();
    }

    size_t completed_args = args.empty() ? 0 : args.size() - 1;

    std::unordered_set<std::string> added;
    long delete_before = at_new_token ? 0 : static_cast<long>(current_token.size());

    auto matches_prefix = [&](const std::string& value) {
        if (current_token.empty()) {
            return true;
        }
        return completion_utils::matches_completion_prefix(value, current_token);
    };

    bool any_added = false;

    if (completed_args == 0) {
        for (const auto& entry : data_copy.subcommands) {
            if (completion_tracker::completion_limit_hit_with_log("external subcommands")) {
                break;
            }
            if (ic_stop_completing(cenv)) {
                break;
            }
            if (!matches_prefix(entry.value)) {
                continue;
            }
            if (!added.insert(entry.value).second) {
                continue;
            }

            std::string insert_text = entry.value;
            if (!insert_text.empty() && insert_text.back() != ' ') {
                insert_text.push_back(' ');
            }

            const std::string& source =
                entry.description.empty() ? data_copy.summary : entry.description;
            if (!completion_tracker::safe_add_completion_prim_with_source(
                    cenv, insert_text.c_str(), nullptr, nullptr, source.c_str(), delete_before,
                    0)) {
                break;
            }
            any_added = true;
        }
    }

    bool allow_flags = true;
    for (const auto& arg : args) {
        if (arg == "--") {
            allow_flags = false;
            break;
        }
    }

    if (allow_flags) {
        for (const auto& entry : data_copy.flags) {
            if (completion_tracker::completion_limit_hit_with_log("external flags")) {
                break;
            }
            if (ic_stop_completing(cenv)) {
                break;
            }
            if (!matches_prefix(entry.value)) {
                continue;
            }
            if (!added.insert(entry.value).second) {
                continue;
            }

            std::string insert_text = entry.value;
            if (!insert_text.empty() && insert_text.find('=') == std::string::npos &&
                insert_text.back() != ' ') {
                insert_text.push_back(' ');
            }

            const std::string& source =
                entry.description.empty() ? data_copy.summary : entry.description;
            if (!completion_tracker::safe_add_completion_prim_with_source(
                    cenv, insert_text.c_str(), nullptr, nullptr, source.c_str(), delete_before,
                    0)) {
                break;
            }
            any_added = true;
        }
    }

    return any_added;
}

void CompletionManager::ensure_directory() {
    std::error_code ec;
    std::filesystem::create_directories(cjsh_filesystem::g_cjsh_completions_path, ec);
}

void CompletionManager::load_existing_files() {
    std::error_code ec;
    std::filesystem::directory_iterator it(cjsh_filesystem::g_cjsh_completions_path, ec);
    std::filesystem::directory_iterator end;
    for (; it != end; it.increment(ec)) {
        if (ec) {
            break;
        }
        const auto& entry = *it;
        if (!entry.is_regular_file()) {
            continue;
        }
        const auto extension = entry.path().extension();
        if (extension == ".cjsh") {
            load_completion_file(entry.path());
            continue;
        }
        if (extension == ".nocjsh") {
            load_no_completion_file(entry.path());
            continue;
        }
    }
}

void CompletionManager::load_completion_file(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return;
    }

    CommandCompletionData data;
    data.file_path = path;

    std::string line;
    while (std::getline(file, line)) {
        line = trim_copy(line);
        if (line.empty() || line[0] == '#') {
            continue;
        }

        std::istringstream iss(line);
        std::string keyword;
        if (!(iss >> keyword)) {
            continue;
        }

        std::string value;
        std::string description;
        if (!(iss >> std::quoted(value))) {
            value.clear();
        }
        if (!(iss >> std::quoted(description))) {
            description.clear();
        }

        if (keyword == "command") {
            data.command = value.empty() ? path.stem().string() : value;
            data.summary = description;
        } else if (keyword == "summary") {
            data.summary = value;
        } else if (keyword == "flag") {
            data.flags.push_back({value, description});
        } else if (keyword == "subcommand") {
            data.subcommands.push_back({value, description});
        }
    }

    if (data.command.empty()) {
        data.command = path.stem().string();
    }

    if (data.summary.empty()) {
        data.summary = "man";
    }

    std::lock_guard<std::mutex> lock(mutex_);
    cache_[data.command] = std::move(data);
}

void CompletionManager::load_no_completion_file(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return;
    }

    std::string command;
    std::chrono::system_clock::time_point recorded_time{};

    std::string line;
    while (std::getline(file, line)) {
        line = trim_copy(line);
        if (line.empty() || line[0] == '#') {
            continue;
        }

        std::istringstream iss(line);
        std::string keyword;
        if (!(iss >> keyword)) {
            continue;
        }

        if (keyword == "command") {
            std::string value;
            if (iss >> std::quoted(value)) {
                command = value;
            }
        } else if (keyword == "generated") {
            std::string value;
            if (iss >> std::quoted(value)) {
                try {
                    long long seconds = std::stoll(value);
                    recorded_time =
                        std::chrono::system_clock::from_time_t(static_cast<std::time_t>(seconds));
                } catch (...) {
                    recorded_time = std::chrono::system_clock::time_point{};
                }
            }
        }
    }

    if (command.empty()) {
        command = path.stem().string();
    }

    auto now = std::chrono::system_clock::now();
    bool expired = recorded_time.time_since_epoch().count() == 0 ||
                   now - recorded_time >= kNoCompletionRetryDelay;

    if (expired) {
        std::error_code ec;
        std::filesystem::remove(path, ec);
        return;
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        no_completion_commands_[command] = recorded_time;
        persistent_negative_commands_.insert(command);
    }
}

void CompletionManager::scan_path_for_changes() {
    auto executables = cjsh_filesystem::get_executables_in_path();
    std::unordered_set<std::string> executables_set(executables.begin(), executables.end());

    remove_missing_commands(executables_set);
}

void CompletionManager::remove_missing_commands(
    const std::unordered_set<std::string>& commands_in_path) {
    std::vector<std::string> to_remove;
    std::vector<std::string> negative_to_remove;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& [command, data] : cache_) {
            if (commands_in_path.find(command) == commands_in_path.end()) {
                to_remove.push_back(command);
            }
        }
        for (const auto& command : persistent_negative_commands_) {
            if (commands_in_path.find(command) == commands_in_path.end()) {
                negative_to_remove.push_back(command);
            }
        }
        for (const auto& command : to_remove) {
            auto it = cache_.find(command);
            if (it != cache_.end()) {
                std::error_code ec;
                std::filesystem::remove(it->second.file_path, ec);
                cache_.erase(it);
            }
            no_completion_commands_.erase(command);
        }
        for (const auto& command : negative_to_remove) {
            no_completion_commands_.erase(command);
        }
        for (const auto& command : negative_to_remove) {
            persistent_negative_commands_.erase(command);
        }
    }

    for (const auto& command : negative_to_remove) {
        std::error_code ignore_ec;
        std::filesystem::remove(command_to_negative_path(command), ignore_ec);
    }

    if (!to_remove.empty()) {
        std::lock_guard<std::mutex> queue_lock(queue_mutex_);
        for (const auto& command : to_remove) {
            pending_commands_.erase(command);
        }
    }
}

void CompletionManager::generate_for_command(const std::string& command) {
    auto data_opt = parse_man_page_for_command(command);
    if (!data_opt.has_value()) {
        mark_no_completion(command, std::chrono::system_clock::now());
        return;
    }

    CommandCompletionData data = std::move(data_opt.value());
    data.file_path = command_to_path(command);

    if (!write_completion_file(data)) {
        return;
    }

    clear_no_completion(data.command);

    std::lock_guard<std::mutex> lock(mutex_);
    cache_[data.command] = std::move(data);
}

std::filesystem::path CompletionManager::command_to_path(const std::string& command) const {
    return cjsh_filesystem::g_cjsh_completions_path / (sanitize_for_filename(command) + ".cjsh");
}

std::filesystem::path CompletionManager::command_to_negative_path(
    const std::string& command) const {
    return cjsh_filesystem::g_cjsh_completions_path / (sanitize_for_filename(command) + ".nocjsh");
}

bool CompletionManager::write_completion_file(const CommandCompletionData& data) {
    std::filesystem::path path = data.file_path;
    std::ofstream file(path, std::ios::out | std::ios::trunc);
    if (!file.is_open()) {
        return false;
    }

    auto now = std::chrono::system_clock::now();
    auto now_time = std::chrono::system_clock::to_time_t(now);

    file << "# Generated by cjsh completion generator\n";
    file << "command " << std::quoted(data.command) << ' ' << std::quoted(data.summary) << "\n";
    file << "generated " << std::quoted(std::to_string(now_time)) << " \"\"\n";

    for (const auto& entry : data.subcommands) {
        file << "subcommand " << std::quoted(entry.value) << ' ' << std::quoted(entry.description)
             << "\n";
    }
    for (const auto& entry : data.flags) {
        file << "flag " << std::quoted(entry.value) << ' ' << std::quoted(entry.description)
             << "\n";
    }

    return true;
}

void CompletionManager::mark_no_completion(const std::string& command,
                                           const std::chrono::system_clock::time_point& when) {
    const auto completion_path = command_to_path(command);
    const auto negative_path = command_to_negative_path(command);

    {
        std::lock_guard<std::mutex> lock(mutex_);
        cache_.erase(command);
        no_completion_commands_[command] = when;
        persistent_negative_commands_.insert(command);
    }

    std::error_code ignore_ec;
    std::filesystem::remove(completion_path, ignore_ec);

    std::ofstream file(negative_path, std::ios::out | std::ios::trunc);
    if (!file.is_open()) {
        return;
    }

    const auto seconds = std::chrono::system_clock::to_time_t(when);
    file << "# No cjsh completions generated\n";
    file << "command " << std::quoted(command) << " \"\"\n";
    file << "generated " << std::quoted(std::to_string(seconds)) << " \"\"\n";
}

void CompletionManager::clear_no_completion(const std::string& command) {
    const auto negative_path = command_to_negative_path(command);

    {
        std::lock_guard<std::mutex> lock(mutex_);
        no_completion_commands_.erase(command);
        persistent_negative_commands_.erase(command);
    }

    std::error_code ignore_ec;
    std::filesystem::remove(negative_path, ignore_ec);
}

std::optional<CommandCompletionData> CompletionManager::parse_man_page_for_command(
    const std::string& command) {
    auto man_text_opt = fetch_man_page(command);
    if (!man_text_opt.has_value()) {
        return std::nullopt;
    }

    std::vector<std::string> lines;
    {
        std::istringstream iss(man_text_opt.value());
        std::string line;
        while (std::getline(iss, line)) {
            lines.push_back(rtrim_copy(line));
        }
    }

    CommandCompletionData data;
    data.command = command;

    Section current = Section::None;
    OptionLine pending_option;
    bool has_pending_option = false;
    SubcommandLine pending_subcommand;
    bool has_pending_subcommand = false;
    bool captured_summary = false;

    std::unordered_set<std::string> seen_flags;
    std::unordered_set<std::string> seen_subcommands;

    auto flush_option = [&]() {
        if (!has_pending_option) {
            return;
        }
        for (const auto& token : pending_option.tokens) {
            if (seen_flags.insert(token).second) {
                data.flags.push_back({token, truncate_description(pending_option.description)});
            }
        }
        has_pending_option = false;
        pending_option = OptionLine{};
    };

    auto flush_subcommand = [&]() {
        if (!has_pending_subcommand) {
            return;
        }
        if (!pending_subcommand.token.empty() &&
            seen_subcommands.insert(pending_subcommand.token).second) {
            data.subcommands.push_back(
                {pending_subcommand.token, truncate_description(pending_subcommand.description)});
        }
        has_pending_subcommand = false;
        pending_subcommand = SubcommandLine{};
    };

    for (size_t i = 0; i < lines.size(); ++i) {
        const std::string& line = lines[i];

        if (is_section_heading(line)) {
            flush_option();
            flush_subcommand();
            current = map_section(line);
            continue;
        }

        if (line.empty()) {
            flush_option();
            flush_subcommand();
            continue;
        }

        switch (current) {
            case Section::Name: {
                if (!captured_summary) {
                    std::string trimmed = trim_copy(line);
                    size_t dash = trimmed.find(" - ");
                    if (dash != std::string::npos) {
                        data.summary = trim_copy(trimmed.substr(dash + 3));
                        captured_summary = true;
                    }
                }
                break;
            }
            case Section::Options: {
                OptionLine parsed = parse_option_line(line);
                if (parsed.valid) {
                    flush_option();
                    pending_option = std::move(parsed);
                    has_pending_option = true;
                } else if (has_pending_option && line.find_first_not_of(' ') != std::string::npos) {
                    std::string continuation = trim_copy(line);
                    if (!continuation.empty()) {
                        if (!pending_option.description.empty()) {
                            pending_option.description.append(" ");
                        }
                        pending_option.description.append(continuation);
                    }
                }
                break;
            }
            case Section::Commands: {
                SubcommandLine parsed = parse_subcommand_line(line, command);
                if (parsed.valid) {
                    flush_subcommand();
                    pending_subcommand = std::move(parsed);
                    has_pending_subcommand = true;
                } else if (has_pending_subcommand &&
                           line.find_first_not_of(' ') != std::string::npos) {
                    std::string continuation = trim_copy(line);
                    if (!continuation.empty()) {
                        if (!pending_subcommand.description.empty()) {
                            pending_subcommand.description.append(" ");
                        }
                        pending_subcommand.description.append(continuation);
                    }
                }
                break;
            }
            default:
                break;
        }
    }

    flush_option();
    flush_subcommand();

    if (data.summary.empty()) {
        data.summary = "man";
    }

    if (data.flags.empty() && data.subcommands.empty()) {
        return std::nullopt;
    }

    return data;
}

}  // namespace

static CompletionManager& manager() {
    static CompletionManager instance;
    return instance;
}

void initialize() {
    manager().initialize();
}

bool add_completions(ic_completion_env_t* cenv, const std::string& command,
                     const std::vector<std::string>& args, bool at_new_token) {
    return manager().add_completions(cenv, command, args, at_new_token);
}

}  // namespace external_argument_completion
