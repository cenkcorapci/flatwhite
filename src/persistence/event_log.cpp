#include "localagent/persistence/event_log.hpp"

#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace localagent {
namespace {

[[nodiscard]] std::string format_timestamp(
    const std::chrono::system_clock::time_point& timestamp) {
  const std::time_t tt = std::chrono::system_clock::to_time_t(timestamp);
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

[[nodiscard]] std::chrono::system_clock::time_point parse_timestamp(
    const std::string& text) {
  std::tm tm{};
  std::istringstream in(text);
  in >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
  if (in.fail()) {
    return std::chrono::system_clock::now();
  }
#if defined(_WIN32)
  const std::time_t tt = _mkgmtime(&tm);
#else
  const std::time_t tt = timegm(&tm);
#endif
  return std::chrono::system_clock::from_time_t(tt);
}

[[nodiscard]] bool is_terminal_event(EventType type) {
  switch (type) {
    case EventType::RunCompleted:
    case EventType::RunFailed:
    case EventType::RunCancelled:
      return true;
    default:
      return false;
  }
}

}  // namespace

std::string to_string(EventType type) {
  switch (type) {
    case EventType::SessionStarted:
      return "SessionStarted";
    case EventType::UserMessage:
      return "UserMessage";
    case EventType::AssistantMessage:
      return "AssistantMessage";
    case EventType::ModeChanged:
      return "ModeChanged";
    case EventType::ModelSelected:
      return "ModelSelected";
    case EventType::ToolRequested:
      return "ToolRequested";
    case EventType::ToolCompleted:
      return "ToolCompleted";
    case EventType::VerificationFailed:
      return "VerificationFailed";
    case EventType::VerificationPassed:
      return "VerificationPassed";
    case EventType::RunCompleted:
      return "RunCompleted";
    case EventType::RunFailed:
      return "RunFailed";
    case EventType::RunCancelled:
      return "RunCancelled";
    case EventType::StateChanged:
      return "StateChanged";
    case EventType::BudgetWarning:
      return "BudgetWarning";
    case EventType::ErrorOccurred:
      return "ErrorOccurred";
  }
  return "ErrorOccurred";
}

std::optional<EventType> parse_event_type(std::string_view text) {
  static const std::pair<std::string_view, EventType> mappings[] = {
      {"SessionStarted", EventType::SessionStarted},
      {"UserMessage", EventType::UserMessage},
      {"AssistantMessage", EventType::AssistantMessage},
      {"ModeChanged", EventType::ModeChanged},
      {"ModelSelected", EventType::ModelSelected},
      {"ToolRequested", EventType::ToolRequested},
      {"ToolCompleted", EventType::ToolCompleted},
      {"VerificationFailed", EventType::VerificationFailed},
      {"VerificationPassed", EventType::VerificationPassed},
      {"RunCompleted", EventType::RunCompleted},
      {"RunFailed", EventType::RunFailed},
      {"RunCancelled", EventType::RunCancelled},
      {"StateChanged", EventType::StateChanged},
      {"BudgetWarning", EventType::BudgetWarning},
      {"ErrorOccurred", EventType::ErrorOccurred},
  };
  for (const auto& [name, type] : mappings) {
    if (name == text) {
      return type;
    }
  }
  return std::nullopt;
}

nlohmann::json Event::to_json() const {
  return nlohmann::json{
      {"id", id.str()},
      {"run_id", run_id.str()},
      {"timestamp", format_timestamp(timestamp)},
      {"type", to_string(type)},
      {"payload", payload},
  };
}

Event Event::from_json(const nlohmann::json& json) {
  Event event;
  event.id = EventId{json.value("id", "")};
  event.run_id = RunId{json.value("run_id", "")};
  event.timestamp = parse_timestamp(json.value("timestamp", ""));
  const auto type_text = json.value("type", std::string{});
  event.type = parse_event_type(type_text).value_or(EventType::ErrorOccurred);
  event.payload = json.value("payload", nlohmann::json::object());
  return event;
}

EventLog::EventLog(std::filesystem::path path) {
  jsonl_path_ = std::move(path);
  if (jsonl_path_.extension() == ".db") {
    jsonl_path_.replace_extension(".jsonl");
  }
  ensure_parent_dir();
}

EventLog::EventLog(std::filesystem::path jsonl_path, SqliteStore* sqlite_store)
    : jsonl_path_(std::move(jsonl_path)), sqlite_store_(sqlite_store) {
  ensure_parent_dir();
}

void EventLog::ensure_parent_dir() const {
  if (jsonl_path_.empty()) {
    throw std::runtime_error("event log path is empty");
  }
  const auto parent = jsonl_path_.parent_path();
  if (!parent.empty()) {
    std::filesystem::create_directories(parent);
  }
}

void EventLog::write_jsonl(const Event& event) const {
  ensure_parent_dir();
  std::ofstream out(jsonl_path_, std::ios::app);
  if (!out) {
    throw std::runtime_error("failed to open event log file for append");
  }
  out << event.to_json().dump() << '\n';
}

void EventLog::mirror_sqlite(const Event& event) const {
  if (sqlite_store_ == nullptr || !sqlite_store_->is_open()) {
    return;
  }
  sqlite_store_->append_event(event.run_id, to_string(event.type),
                              event.payload.dump());
}

Event EventLog::append(const Event& event) {
  Event stored = event;
  if (stored.id.empty()) {
    stored.id = EventId{make_uuid()};
  }
  if (stored.timestamp == std::chrono::system_clock::time_point{}) {
    stored.timestamp = std::chrono::system_clock::now();
  }
  write_jsonl(stored);
  mirror_sqlite(stored);
  return stored;
}

Event EventLog::append(RunId run_id, EventType type, nlohmann::json payload) {
  Event event;
  event.id = EventId{make_uuid()};
  event.run_id = std::move(run_id);
  event.timestamp = std::chrono::system_clock::now();
  event.type = type;
  event.payload = std::move(payload);
  return append(event);
}

void EventLog::append(const RunId& run_id, std::string type, std::string payload_json) {
  nlohmann::json payload = nlohmann::json::object();
  if (!payload_json.empty()) {
    payload = nlohmann::json::parse(payload_json);
  }
  append(run_id, parse_event_type(type).value_or(EventType::ErrorOccurred),
         std::move(payload));
}

std::vector<Event> EventLog::list_for_run(const RunId& run_id) const {
  std::vector<Event> events;
  if (jsonl_path_.empty() || !std::filesystem::exists(jsonl_path_)) {
    return events;
  }

  std::ifstream in(jsonl_path_);
  if (!in) {
    throw std::runtime_error("failed to open event log file for read");
  }

  std::string line;
  while (std::getline(in, line)) {
    if (line.empty()) {
      continue;
    }
    const auto json = nlohmann::json::parse(line);
    Event event = Event::from_json(json);
    if (event.run_id == run_id) {
      events.push_back(std::move(event));
    }
  }
  return events;
}

std::vector<RunId> EventLog::find_incomplete_runs() const {
  std::unordered_map<std::string, bool> terminal_by_run;
  std::vector<RunId> order;

  if (jsonl_path_.empty() || !std::filesystem::exists(jsonl_path_)) {
    return {};
  }

  std::ifstream in(jsonl_path_);
  if (!in) {
    throw std::runtime_error("failed to open event log file for read");
  }

  std::string line;
  while (std::getline(in, line)) {
    if (line.empty()) {
      continue;
    }
    const Event event = Event::from_json(nlohmann::json::parse(line));
    const std::string key = event.run_id.str();
    if (key.empty()) {
      continue;
    }
    if (!terminal_by_run.contains(key)) {
      terminal_by_run.emplace(key, false);
      order.push_back(event.run_id);
    }
    if (is_terminal_event(event.type)) {
      terminal_by_run[key] = true;
    }
  }

  std::vector<RunId> incomplete;
  for (const auto& run_id : order) {
    const auto it = terminal_by_run.find(run_id.str());
    if (it != terminal_by_run.end() && !it->second) {
      incomplete.push_back(run_id);
    }
  }
  return incomplete;
}

}  // namespace localagent
