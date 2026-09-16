#include "localagent/persistence/event_log.hpp"
#include "localagent/persistence/sqlite_store.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>

namespace {

std::filesystem::path temp_event_paths() {
  const auto dir = std::filesystem::temp_directory_path();
  const auto stem = "localagent_event_test_" + localagent::make_uuid();
  return dir / stem;
}

}  // namespace

TEST_CASE("EventLog appends JSONL and mirrors sqlite metadata", "[persistence][event_log]") {
  const auto base = temp_event_paths();
  const auto jsonl_path = base.string() + ".jsonl";
  const auto db_path = base.string() + ".db";

  localagent::SqliteStore store;
  store.open(db_path);
  const auto session_id = store.create_session("event session");
  const auto run = store.create_run(session_id, "run goal");

  localagent::EventLog log(jsonl_path, &store);
  log.append(run.id, localagent::EventType::SessionStarted,
             nlohmann::json{{"session_id", session_id.str()}});
  log.append(run.id, localagent::EventType::UserMessage,
             nlohmann::json{{"text", "hello"}});
  log.append(run.id, localagent::EventType::RunCompleted, nlohmann::json::object());

  const auto events = log.list_for_run(run.id);
  REQUIRE(events.size() == 3);
  REQUIRE(events[0].type == localagent::EventType::SessionStarted);
  REQUIRE(events[1].payload["text"] == "hello");
  REQUIRE(events[2].type == localagent::EventType::RunCompleted);

  const auto sqlite_events = store.list_events_for_run(run.id);
  REQUIRE(sqlite_events.size() == 3);

  const auto incomplete = log.find_incomplete_runs();
  REQUIRE(incomplete.empty());

  store.close();
  std::filesystem::remove(jsonl_path);
  std::filesystem::remove(db_path);
}

TEST_CASE("EventLog finds incomplete runs", "[persistence][event_log]") {
  const auto base = temp_event_paths();
  const auto jsonl_path = base.string() + ".jsonl";

  localagent::EventLog log(jsonl_path);
  const localagent::RunId complete_run{"run-complete"};
  const localagent::RunId incomplete_run{"run-incomplete"};

  log.append(complete_run, localagent::EventType::SessionStarted);
  log.append(complete_run, localagent::EventType::RunCompleted);
  log.append(incomplete_run, localagent::EventType::SessionStarted);
  log.append(incomplete_run, localagent::EventType::ToolRequested,
             nlohmann::json{{"tool", "grep"}});

  const auto incomplete = log.find_incomplete_runs();
  REQUIRE(incomplete.size() == 1);
  REQUIRE(incomplete.front() == incomplete_run);

  std::filesystem::remove(jsonl_path);
}
