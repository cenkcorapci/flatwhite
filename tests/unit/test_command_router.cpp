#include "localagent/cli/command_router.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace localagent;

TEST_CASE("CommandRouter parses modes and prompt flag", "[command_router]") {
  CommandRouter router;
  const char* argv[] = {"flatwhite", "agent", "-p", "fix bug", "--output-format", "json"};
  const auto cmd = router.parse(6, const_cast<char**>(argv));
  CHECK(cmd.kind == CommandKind::Prompt);
  CHECK(cmd.mode == AgentMode::Agent);
  CHECK(cmd.prompt == "fix bug");
  CHECK(cmd.output_format == OutputFormat::Json);
}

TEST_CASE("CommandRouter handles sessions subcommands", "[command_router]") {
  CommandRouter router;
  const char* argv[] = {"flatwhite", "sessions", "list"};
  const auto cmd = router.parse(3, const_cast<char**>(argv));
  CHECK(cmd.kind == CommandKind::SessionsList);
}

TEST_CASE("CommandRouter help and version", "[command_router]") {
  CHECK_FALSE(CommandRouter::help_text().empty());
  CHECK(CommandRouter::version_text().find("0.1.0") != std::string::npos);
}

TEST_CASE("CommandRouter starts interactive mode without arguments", "[command_router]") {
  CommandRouter router;
  const char* argv[] = {"flatwhite"};
  CHECK(router.parse(1, const_cast<char**>(argv)).kind == CommandKind::Interactive);
}
