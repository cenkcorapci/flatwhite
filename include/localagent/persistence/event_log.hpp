#pragma once

#include "localagent/common/strong_id.hpp"
#include "localagent/persistence/sqlite_store.hpp"

#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

namespace localagent {

enum class EventType {
  SessionStarted,
  UserMessage,
  AssistantMessage,
  ModeChanged,
  ModelSelected,
  ToolRequested,
  ToolCompleted,
  VerificationFailed,
  VerificationPassed,
  RunCompleted,
  RunFailed,
  RunCancelled,
  StateChanged,
  BudgetWarning,
  ErrorOccurred
};

[[nodiscard]] std::string to_string(EventType type);
[[nodiscard]] std::optional<EventType> parse_event_type(std::string_view text);

struct Event {
  EventId id;
  RunId run_id;
  std::chrono::system_clock::time_point timestamp{};
  EventType type{EventType::SessionStarted};
  nlohmann::json payload = nlohmann::json::object();

  [[nodiscard]] nlohmann::json to_json() const;
  [[nodiscard]] static Event from_json(const nlohmann::json& json);
};

class EventLog {
public:
  EventLog() = default;
  explicit EventLog(std::filesystem::path path);
  EventLog(std::filesystem::path jsonl_path,
           SqliteStore* sqlite_store);

  [[nodiscard]] const std::filesystem::path& path() const noexcept {
    return jsonl_path_;
  }

  Event append(const Event& event);
  Event append(RunId run_id, EventType type,
               nlohmann::json payload = nlohmann::json::object());
  void append(const RunId& run_id, std::string type, std::string payload_json);

  [[nodiscard]] std::vector<Event> list_for_run(const RunId& run_id) const;
  [[nodiscard]] std::vector<RunId> find_incomplete_runs() const;

private:
  void ensure_parent_dir() const;
  void write_jsonl(const Event& event) const;
  void mirror_sqlite(const Event& event) const;

  std::filesystem::path jsonl_path_;
  SqliteStore* sqlite_store_{nullptr};
};

}  // namespace localagent
