#pragma once

#include "localagent/common/strong_id.hpp"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

struct sqlite3;

namespace localagent {

class SqliteStatement;

class SqliteConnection {
public:
  SqliteConnection();
  ~SqliteConnection();

  SqliteConnection(const SqliteConnection&) = delete;
  SqliteConnection& operator=(const SqliteConnection&) = delete;
  SqliteConnection(SqliteConnection&& other) noexcept;
  SqliteConnection& operator=(SqliteConnection&& other) noexcept;

  [[nodiscard]] sqlite3* handle() const noexcept { return db_; }
  [[nodiscard]] bool is_open() const noexcept { return db_ != nullptr; }

  void open(const std::string& path);
  void close();
  void exec(const std::string& sql);
  [[nodiscard]] SqliteStatement prepare(const std::string& sql);

private:
  friend class SqliteStatement;
  sqlite3* db_{nullptr};
};

class SqliteStatement {
public:
  SqliteStatement();
  ~SqliteStatement();

  SqliteStatement(const SqliteStatement&) = delete;
  SqliteStatement& operator=(const SqliteStatement&) = delete;
  SqliteStatement(SqliteStatement&& other) noexcept;
  SqliteStatement& operator=(SqliteStatement&& other) noexcept;

  [[nodiscard]] bool is_valid() const noexcept { return stmt_ != nullptr; }
  void bind_int(int index, int value);
  void bind_int64(int index, long long value);
  void bind_text(int index, const std::string& value);
  void bind_null(int index);
  bool step();
  void reset();
  [[nodiscard]] std::string column_text(int index) const;

private:
  friend class SqliteConnection;
  SqliteStatement(sqlite3* db, void* stmt);
  sqlite3* db_{nullptr};
  void* stmt_{nullptr};
};

struct SessionRecord {
  SessionId id;
  std::string title;
  std::string workspace_root;
  std::string created_at;
  std::string updated_at;
};

struct RunRecord {
  RunId id;
  SessionId session_id;
  std::string state;
  std::string goal;
  std::string created_at;
  std::string updated_at;
};

struct MessageRecord {
  MessageId id;
  RunId run_id;
  std::string role;
  std::string content;
  std::string created_at;
};

struct EventRecord {
  EventId id;
  RunId run_id;
  std::string event_type;
  std::string payload_json;
  std::string created_at;
};

struct ToolCallRecord {
  ToolCallId id;
  RunId run_id;
  std::string tool_name;
  std::string input_json;
  std::optional<std::string> output_path;
  std::string status;
  std::string created_at;
  std::optional<std::string> completed_at;
};

class SqliteStore {
public:
  SqliteStore() = default;
  explicit SqliteStore(std::filesystem::path db_path);

  void open(const std::string& path);
  void close();

  [[nodiscard]] bool is_open() const noexcept { return conn_.is_open(); }

  [[nodiscard]] SessionId create_session(std::string title);
  SessionRecord create_session(std::string title, std::string workspace_root);
  void touch_session(const SessionId& id);
  [[nodiscard]] std::optional<SessionRecord> get_session(const SessionId& id) const;

  RunRecord create_run(const SessionId& session_id, std::string goal,
                       std::string state = "running");
  void update_run_state(const RunId& id, std::string state);

  MessageRecord append_message(const RunId& run_id, std::string role,
                               std::string content);

  EventRecord append_event(const RunId& run_id, std::string event_type,
                           std::string payload_json);

  ToolCallRecord create_tool_call(const RunId& run_id, std::string tool_name,
                                  std::string input_json,
                                  std::optional<std::string> output_path = std::nullopt);

  void complete_tool_call(const ToolCallId& id, std::string status,
                          std::optional<std::string> output_path = std::nullopt);

  [[nodiscard]] std::vector<SessionRecord> list_sessions(std::size_t limit = 0) const;
  [[nodiscard]] std::vector<RunRecord> list_runs_for_session(
      const SessionId& session_id) const;
  [[nodiscard]] std::vector<MessageRecord> list_messages_for_run(
      const RunId& run_id) const;
  [[nodiscard]] std::vector<EventRecord> list_events_for_run(
      const RunId& run_id) const;
  [[nodiscard]] std::vector<RunRecord> list_runs_by_state(
      const std::string& state) const;

private:
  void initialize_schema();
  [[nodiscard]] static std::string now_iso8601();

  SqliteConnection conn_;
  std::string path_;
};

}  // namespace localagent
