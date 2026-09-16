#include "localagent/persistence/sqlite_store.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>

namespace {

std::filesystem::path temp_db_path() {
  const auto dir = std::filesystem::temp_directory_path();
  return dir / ("localagent_sqlite_test_" + localagent::make_uuid() + ".db");
}

}  // namespace

TEST_CASE("SqliteStore creates sessions and runs", "[persistence][sqlite]") {
  const auto db_path = temp_db_path();
  localagent::SqliteStore store;
  store.open(db_path.string());

  const auto session = store.create_session("test session", "/tmp/workspace");
  REQUIRE_FALSE(session.id.empty());
  REQUIRE(session.title == "test session");
  REQUIRE(session.workspace_root == "/tmp/workspace");

  const auto fetched = store.get_session(session.id);
  REQUIRE(fetched.has_value());
  REQUIRE(fetched->id == session.id);

  const auto run = store.create_run(session.id, "implement feature");
  REQUIRE_FALSE(run.id.empty());
  REQUIRE(run.state == "running");

  store.update_run_state(run.id, "completed");
  const auto runs = store.list_runs_for_session(session.id);
  REQUIRE(runs.size() == 1);
  REQUIRE(runs.front().state == "completed");

  const auto message = store.append_message(run.id, "user", "hello");
  REQUIRE(message.role == "user");
  REQUIRE(message.content == "hello");

  const auto messages = store.list_messages_for_run(run.id);
  REQUIRE(messages.size() == 1);

  const auto tool_call = store.create_tool_call(run.id, "read_file", R"({"path":"a.txt"})",
                                                std::optional<std::string>{"out.txt"});
  REQUIRE(tool_call.status == "pending");
  store.complete_tool_call(tool_call.id, "completed");

  const auto sessions = store.list_sessions();
  REQUIRE(sessions.size() == 1);
  REQUIRE(sessions.front().id == session.id);

  store.close();
  std::filesystem::remove(db_path);
}
