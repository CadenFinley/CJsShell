#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class CronJob {
public:
    std::string id;
    std::string name;
    std::string schedule;
    std::string scriptName;
    bool enabled;
    std::string lastRun;
    std::string nextRun;
    
    CronJob() : enabled(false) {}
    
    CronJob(const std::string& id, const std::string& name, const std::string& schedule,
            const std::string& scriptName, bool enabled = true,
            const std::string& lastRun = "", const std::string& nextRun = "")
        : id(id), name(name), schedule(schedule), scriptName(scriptName), 
          enabled(enabled), lastRun(lastRun), nextRun(nextRun) {}
    
    json toJson() const {
        json j;
        j["id"] = id;
        j["name"] = name;
        j["schedule"] = schedule;
        j["script_name"] = scriptName;
        j["enabled"] = enabled;
        if (!lastRun.empty()) j["last_run"] = lastRun;
        if (!nextRun.empty()) j["next_run"] = nextRun;
        return j;
    }
    
    static CronJob fromJson(const json& j) {
        CronJob job;
        if (j.contains("id")) job.id = j["id"].get<std::string>();
        if (j.contains("name")) job.name = j["name"].get<std::string>();
        if (j.contains("schedule")) job.schedule = j["schedule"].get<std::string>();
        if (j.contains("script_name")) job.scriptName = j["script_name"].get<std::string>();
        if (j.contains("enabled")) job.enabled = j["enabled"].get<bool>();
        if (j.contains("last_run")) job.lastRun = j["last_run"].get<std::string>();
        if (j.contains("next_run")) job.nextRun = j["next_run"].get<std::string>();
        return job;
    }
};
