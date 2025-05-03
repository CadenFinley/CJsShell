#define CATCH_CONFIG_MAIN
#include "vendor/catch.hpp"
#include "src/utils/parser.h"

TEST_CASE("Parser::parse_command splits words and env vars", "[parser]") {
    std::string cmd = "echo $HOME and /tmp";
    auto parts = Parser::parse_command(cmd);
    REQUIRE(parts.size() >= 3);
    REQUIRE(parts[0] == "echo");
    REQUIRE(parts[1].find("/") != std::string::npos); // HOME expanded or literal
}

TEST_CASE("Parser::is_env_assignment detects valid assignments", "[parser]") {
    std::string name, value;
    bool ok1 = Parser::is_env_assignment("FOO=bar", name, value);
    REQUIRE(ok1);
    REQUIRE(name == "FOO");
    REQUIRE(value == "bar");

    bool ok2 = Parser::is_env_assignment("1BAD=val", name, value);
    REQUIRE_FALSE(ok2);
}
