#include "localagent/persistence/sqlite_store.hpp"

#include <chrono>
#include <filesystem>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdexcept>

#include <sqlite3.h>

namespace localagent {
namespace {

[[noreturn]] void throw_sqlite_error(sqlite3* db, const char* context) {
  const char* message = db != nullptr ? sqlite3_errmsg(db) : "unknown sqlite error";
  throw std::runtime_error(std::string(context) + ": " + message);
}

}  // namespace

SqliteConnection::SqliteConnection() = default;

SqliteConnection::~SqliteConnection() {
  close();
}

SqliteConnection::SqliteConnection(SqliteConnection&& other) noexcept
    : db_(other.db_) {
  other.db_ = nullptr;
}

SqliteConnection& SqliteConnection::operator=(SqliteConnection&& other) noexcept {
  if (this != &other) {
    close();
    db_ = other.db_;
    other.db_ = nullptr;
  }
  return *this;
}

void SqliteConnection::open(const std::string& path) {
  close();
  sqlite3* raw = nullptr;
  const int rc = sqlite3_open(path.c_str(), &raw);
  if (rc != SQLITE_OK) {
    const std::string message =
        raw != nullptr ? sqlite3_errmsg(raw) : "failed to open sqlite database";
    if (raw != nullptr) {
      sqlite3_close(raw);
    }
    throw std::runtime_error(message);
  }
  db_ = raw;
}

void SqliteConnection::close() {
  if (db_ != nullptr) {
    sqlite3_close(db_);
    db_ = nullptr;
  }
}

void SqliteConnection::exec(const std::string& sql) {
  char* err = nullptr;
  const int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err);
  if (rc != SQLITE_OK) {
    std::string message = err != nullptr ? err : "sqlite exec failed";
    if (err != nullptr) {
      sqlite3_free(err);
    }
    throw std::runtime_error(message);
  }
}

SqliteStatement SqliteConnection::prepare(const std::string& sql) {
  sqlite3_stmt* raw = nullptr;
  const int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &raw, nullptr);
  if (rc != SQLITE_OK) {
    throw_sqlite_error(db_, "prepare statement");
  }
  return SqliteStatement(db_, raw);
}

SqliteStatement::SqliteStatement() = default;

SqliteStatement::SqliteStatement(sqlite3* db, void* stmt) : db_(db), stmt_(stmt) {}

SqliteStatement::~SqliteStatement() {
  if (stmt_ != nullptr) {
    sqlite3_finalize(static_cast<sqlite3_stmt*>(stmt_));
    stmt_ = nullptr;
  }
}

SqliteStatement::SqliteStatement(SqliteStatement&& other) noexcept
    : db_(other.db_), stmt_(other.stmt_) {
  other.stmt_ = nullptr;
}

SqliteStatement& SqliteStatement::operator=(SqliteStatement&& other) noexcept {
  if (this != &other) {
    if (stmt_ != nullptr) {
      sqlite3_finalize(static_cast<sqlite3_stmt*>(stmt_));
    }
    db_ = other.db_;
    stmt_ = other.stmt_;
    other.stmt_ = nullptr;
  }
  return *this;
}

void SqliteStatement::bind_int(int index, int value) {
  const int rc = sqlite3_bind_int(static_cast<sqlite3_stmt*>(stmt_), index, value);
  if (rc != SQLITE_OK) {
    throw_sqlite_error(db_, "bind int");
  }
}

void SqliteStatement::bind_int64(int index, long long value) {
  const int rc = sqlite3_bind_int64(static_cast<sqlite3_stmt*>(stmt_), index, value);
  if (rc != SQLITE_OK) {
    throw_sqlite_error(db_, "bind int64");
  }
}

void SqliteStatement::bind_text(int index, const std::string& value) {
  const int rc = sqlite3_bind_text(static_cast<sqlite3_stmt*>(stmt_), index, value.c_str(),
                                   -1, SQLITE_TRANSIENT);
  if (rc != SQLITE_OK) {
    throw_sqlite_error(db_, "bind text");
  }
}

void SqliteStatement::bind_null(int index) {
  const int rc = sqlite3_bind_null(static_cast<sqlite3_stmt*>(stmt_), index);
  if (rc != SQLITE_OK) {
    throw_sqlite_error(db_, "bind null");
  }
}

bool SqliteStatement::step() {
  const int rc = sqlite3_step(static_cast<sqlite3_stmt*>(stmt_));
  if (rc == SQLITE_ROW) {
    return true;
  }
  if (rc == SQLITE_DONE) {
    return false;
  }
  throw_sqlite_error(db_, "step statement");
}

void SqliteStatement::reset() {
  const int rc = sqlite3_reset(static_cast<sqlite3_stmt*>(stmt_));
  if (rc != SQLITE_OK) {
    throw_sqlite_error(db_, "reset statement");
  }
  sqlite3_clear_bindings(static_cast<sqlite3_stmt*>(stmt_));
}

std::string SqliteStatement::column_text(int index) const {
  const unsigned char* text =
      sqlite3_column_text(static_cast<sqlite3_stmt*>(stmt_), index);
  return text != nullptr ? reinterpret_cast<const char*>(text) : std::string{};
}

SqliteStore::SqliteStore(std::filesystem::path db_path) {
  open(db_path.string());
}

void SqliteStore::open(const std::string& path) {
  close();
  path_ = path;
  conn_.open(path);
  conn_.exec("PRAGMA journal_mode=WAL;");
  conn_.exec("PRAGMA foreign_keys=ON;");
  initialize_schema();
}

void SqliteStore::close() {
  conn_.close();
  path_.clear();
}

void SqliteStore::initialize_schema() {
  conn_.exec(R"SQL(
    CREATE TABLE IF NOT EXISTS sessions (
      id TEXT PRIMARY KEY,
      title TEXT NOT NULL DEFAULT '',
      workspace_root TEXT NOT NULL DEFAULT '',
      created_at TEXT NOT NULL,
      updated_at TEXT NOT NULL
    );

    CREATE TABLE IF NOT EXISTS runs (
      id TEXT PRIMARY KEY,
      session_id TEXT NOT NULL,
      state TEXT NOT NULL,
      goal TEXT NOT NULL DEFAULT '',
      created_at TEXT NOT NULL,
      updated_at TEXT NOT NULL,
      FOREIGN KEY(session_id) REFERENCES sessions(id)
    );

    CREATE TABLE IF NOT EXISTS messages (
      id TEXT PRIMARY KEY,
      run_id TEXT NOT NULL,
      role TEXT NOT NULL,
      content TEXT NOT NULL,
      created_at TEXT NOT NULL,
      FOREIGN KEY(run_id) REFERENCES runs(id)
    );

    CREATE TABLE IF NOT EXISTS events (
      id TEXT PRIMARY KEY,
      run_id TEXT NOT NULL,
      event_type TEXT NOT NULL,
      payload_json TEXT NOT NULL,
      created_at TEXT NOT NULL,
      FOREIGN KEY(run_id) REFERENCES runs(id)
    );

    CREATE TABLE IF NOT EXISTS tool_calls (
      id TEXT PRIMARY KEY,
      run_id TEXT NOT NULL,
      tool_name TEXT NOT NULL,
      input_json TEXT NOT NULL DEFAULT '{}',
      output_path TEXT,
      status TEXT NOT NULL,
      created_at TEXT NOT NULL,
      completed_at TEXT,
      FOREIGN KEY(run_id) REFERENCES runs(id)
    );

    CREATE INDEX IF NOT EXISTS idx_runs_session_id ON runs(session_id);
    CREATE INDEX IF NOT EXISTS idx_runs_state ON runs(state);
    CREATE INDEX IF NOT EXISTS idx_messages_run_id ON messages(run_id);
    CREATE INDEX IF NOT EXISTS idx_events_run_id ON events(run_id);
    CREATE INDEX IF NOT EXISTS idx_tool_calls_run_id ON tool_calls(run_id);
  )SQL");
}

std::string SqliteStore::now_iso8601() {
  const auto now = std::chrono::system_clock::now();
  const std::time_t tt = std::chrono::system_clock::to_time_t(now);
  std::tm tm{};
#if defined(_WIN32)
  gmtime_s(&tm, &tt);
#else
  gmtime_r(&tt, &tm);
#endif
  std::ostringstream out;
  out << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
  return out.str();
}

SessionId SqliteStore::create_session(std::string title) {
  return create_session(std::move(title), "").id;
}

void SqliteStore::touch_session(const SessionId& id) {
  const std::string now = now_iso8601();
  auto stmt = conn_.prepare("UPDATE sessions SET updated_at = ? WHERE id = ?;");
  stmt.bind_text(1, now);
  stmt.bind_text(2, id.str());
  stmt.step();
}

SessionRecord SqliteStore::create_session(std::string title,
                                          std::string workspace_root) {
  const SessionId id{make_uuid()};
  const std::string now = now_iso8601();

  auto stmt = conn_.prepare(
      "INSERT INTO sessions (id, title, workspace_root, created_at, updated_at) "
      "VALUES (?, ?, ?, ?, ?);");
  stmt.bind_text(1, id.str());
  stmt.bind_text(2, title);
  stmt.bind_text(3, workspace_root);
  stmt.bind_text(4, now);
  stmt.bind_text(5, now);
  stmt.step();

  return SessionRecord{
      .id = id,
      .title = std::move(title),
      .workspace_root = std::move(workspace_root),
      .created_at = now,
      .updated_at = now,
  };
}

std::optional<SessionRecord> SqliteStore::get_session(const SessionId& id) const {
  auto stmt = const_cast<SqliteConnection&>(conn_).prepare(
      "SELECT id, title, workspace_root, created_at, updated_at "
      "FROM sessions WHERE id = ?;");
  stmt.bind_text(1, id.str());
  if (!stmt.step()) {
    return std::nullopt;
  }

  return SessionRecord{
      .id = SessionId{stmt.column_text(0)},
      .title = stmt.column_text(1),
      .workspace_root = stmt.column_text(2),
      .created_at = stmt.column_text(3),
      .updated_at = stmt.column_text(4),
  };
}

RunRecord SqliteStore::create_run(const SessionId& session_id, std::string goal,
                                  std::string state) {
  const RunId id{make_uuid()};
  const std::string now = now_iso8601();

  auto stmt = conn_.prepare(
      "INSERT INTO runs (id, session_id, state, goal, created_at, updated_at) "
      "VALUES (?, ?, ?, ?, ?, ?);");
  stmt.bind_text(1, id.str());
  stmt.bind_text(2, session_id.str());
  stmt.bind_text(3, state);
  stmt.bind_text(4, goal);
  stmt.bind_text(5, now);
  stmt.bind_text(6, now);
  stmt.step();

  return RunRecord{
      .id = id,
      .session_id = session_id,
      .state = state,
      .goal = std::move(goal),
      .created_at = now,
      .updated_at = now,
  };
}

void SqliteStore::update_run_state(const RunId& id, std::string state) {
  const std::string now = now_iso8601();
  auto stmt = conn_.prepare("UPDATE runs SET state = ?, updated_at = ? WHERE id = ?;");
  stmt.bind_text(1, state);
  stmt.bind_text(2, now);
  stmt.bind_text(3, id.str());
  stmt.step();
}

MessageRecord SqliteStore::append_message(const RunId& run_id, std::string role,
                                          std::string content) {
  const MessageId id{make_uuid()};
  const std::string now = now_iso8601();

  auto stmt = conn_.prepare(
      "INSERT INTO messages (id, run_id, role, content, created_at) "
      "VALUES (?, ?, ?, ?, ?);");
  stmt.bind_text(1, id.str());
  stmt.bind_text(2, run_id.str());
  stmt.bind_text(3, role);
  stmt.bind_text(4, content);
  stmt.bind_text(5, now);
  stmt.step();

  return MessageRecord{
      .id = id,
      .run_id = run_id,
      .role = std::move(role),
      .content = std::move(content),
      .created_at = now,
  };
}

EventRecord SqliteStore::append_event(const RunId& run_id, std::string event_type,
                                      std::string payload_json) {
  const EventId id{make_uuid()};
  const std::string now = now_iso8601();

  auto stmt = conn_.prepare(
      "INSERT INTO events (id, run_id, event_type, payload_json, created_at) "
      "VALUES (?, ?, ?, ?, ?);");
  stmt.bind_text(1, id.str());
  stmt.bind_text(2, run_id.str());
  stmt.bind_text(3, event_type);
  stmt.bind_text(4, payload_json);
  stmt.bind_text(5, now);
  stmt.step();

  return EventRecord{
      .id = id,
      .run_id = run_id,
      .event_type = std::move(event_type),
      .payload_json = std::move(payload_json),
      .created_at = now,
  };
}

ToolCallRecord SqliteStore::create_tool_call(const RunId& run_id, std::string tool_name,
                                             std::string input_json,
                                             std::optional<std::string> output_path) {
  const ToolCallId id{make_uuid()};
  const std::string now = now_iso8601();

  auto stmt = conn_.prepare(
      "INSERT INTO tool_calls (id, run_id, tool_name, input_json, output_path, status, "
      "created_at) VALUES (?, ?, ?, ?, ?, ?, ?);");
  stmt.bind_text(1, id.str());
  stmt.bind_text(2, run_id.str());
  stmt.bind_text(3, tool_name);
  stmt.bind_text(4, input_json);
  if (output_path) {
    stmt.bind_text(5, *output_path);
  } else {
    stmt.bind_null(5);
  }
  stmt.bind_text(6, "pending");
  stmt.bind_text(7, now);
  stmt.step();

  return ToolCallRecord{
      .id = id,
      .run_id = run_id,
      .tool_name = std::move(tool_name),
      .input_json = std::move(input_json),
      .output_path = std::move(output_path),
      .status = "pending",
      .created_at = now,
      .completed_at = std::nullopt,
  };
}

void SqliteStore::complete_tool_call(const ToolCallId& id, std::string status,
                                     std::optional<std::string> output_path) {
  const std::string now = now_iso8601();
  auto stmt = conn_.prepare(
      "UPDATE tool_calls SET status = ?, output_path = COALESCE(?, output_path), "
      "completed_at = ? WHERE id = ?;");
  stmt.bind_text(1, status);
  if (output_path) {
    stmt.bind_text(2, *output_path);
  } else {
    stmt.bind_null(2);
  }
  stmt.bind_text(3, now);
  stmt.bind_text(4, id.str());
  stmt.step();
}

std::vector<SessionRecord> SqliteStore::list_sessions(std::size_t limit) const {
  std::vector<SessionRecord> sessions;
  std::string sql =
      "SELECT id, title, workspace_root, created_at, updated_at "
      "FROM sessions ORDER BY created_at DESC";
  if (limit > 0) {
    sql += " LIMIT " + std::to_string(limit);
  }
  sql += ";";
  auto stmt = const_cast<SqliteConnection&>(conn_).prepare(sql);
  while (stmt.step()) {
    sessions.push_back(SessionRecord{
        .id = SessionId{stmt.column_text(0)},
        .title = stmt.column_text(1),
        .workspace_root = stmt.column_text(2),
        .created_at = stmt.column_text(3),
        .updated_at = stmt.column_text(4),
    });
  }
  return sessions;
}

std::vector<RunRecord> SqliteStore::list_runs_for_session(
    const SessionId& session_id) const {
  std::vector<RunRecord> runs;
  auto stmt = const_cast<SqliteConnection&>(conn_).prepare(
      "SELECT id, session_id, state, goal, created_at, updated_at "
      "FROM runs WHERE session_id = ? ORDER BY created_at ASC;");
  stmt.bind_text(1, session_id.str());
  while (stmt.step()) {
    runs.push_back(RunRecord{
        .id = RunId{stmt.column_text(0)},
        .session_id = SessionId{stmt.column_text(1)},
        .state = stmt.column_text(2),
        .goal = stmt.column_text(3),
        .created_at = stmt.column_text(4),
        .updated_at = stmt.column_text(5),
    });
  }
  return runs;
}

std::vector<MessageRecord> SqliteStore::list_messages_for_run(const RunId& run_id) const {
  std::vector<MessageRecord> messages;
  auto stmt = const_cast<SqliteConnection&>(conn_).prepare(
      "SELECT id, run_id, role, content, created_at "
      "FROM messages WHERE run_id = ? ORDER BY created_at ASC;");
  stmt.bind_text(1, run_id.str());
  while (stmt.step()) {
    messages.push_back(MessageRecord{
        .id = MessageId{stmt.column_text(0)},
        .run_id = RunId{stmt.column_text(1)},
        .role = stmt.column_text(2),
        .content = stmt.column_text(3),
        .created_at = stmt.column_text(4),
    });
  }
  return messages;
}

std::vector<EventRecord> SqliteStore::list_events_for_run(const RunId& run_id) const {
  std::vector<EventRecord> events;
  auto stmt = const_cast<SqliteConnection&>(conn_).prepare(
      "SELECT id, run_id, event_type, payload_json, created_at "
      "FROM events WHERE run_id = ? ORDER BY created_at ASC;");
  stmt.bind_text(1, run_id.str());
  while (stmt.step()) {
    events.push_back(EventRecord{
        .id = EventId{stmt.column_text(0)},
        .run_id = RunId{stmt.column_text(1)},
        .event_type = stmt.column_text(2),
        .payload_json = stmt.column_text(3),
        .created_at = stmt.column_text(4),
    });
  }
  return events;
}

std::vector<RunRecord> SqliteStore::list_runs_by_state(const std::string& state) const {
  std::vector<RunRecord> runs;
  auto stmt = const_cast<SqliteConnection&>(conn_).prepare(
      "SELECT id, session_id, state, goal, created_at, updated_at "
      "FROM runs WHERE state = ? ORDER BY created_at ASC;");
  stmt.bind_text(1, state);
  while (stmt.step()) {
    runs.push_back(RunRecord{
        .id = RunId{stmt.column_text(0)},
        .session_id = SessionId{stmt.column_text(1)},
        .state = stmt.column_text(2),
        .goal = stmt.column_text(3),
        .created_at = stmt.column_text(4),
        .updated_at = stmt.column_text(5),
    });
  }
  return runs;
}

}  // namespace localagent
