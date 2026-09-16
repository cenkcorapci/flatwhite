#include "localagent/app/session_controller.hpp"

namespace localagent {

SessionController::SessionController(std::filesystem::path data_dir)
    : store_([&] {
        std::filesystem::create_directories(data_dir);
        return std::make_unique<SqliteStore>(data_dir / "sessions.db");
      }()) {}

SessionId SessionController::create(std::string title) {
  if (title.empty()) {
    title = "session";
  }
  return store_->create_session(std::move(title));
}

std::optional<SessionId> SessionController::resume(const std::optional<std::string>& id,
                                                   bool latest_if_missing) {
  if (id) {
    const SessionId sid{*id};
    if (store_->get_session(sid)) {
      store_->touch_session(sid);
      return sid;
    }
    return std::nullopt;
  }
  if (!latest_if_missing) {
    return std::nullopt;
  }
  const auto sessions = store_->list_sessions(1);
  if (sessions.empty()) {
    return std::nullopt;
  }
  store_->touch_session(sessions.front().id);
  return sessions.front().id;
}

std::vector<SessionRecord> SessionController::list() const { return store_->list_sessions(); }

}  // namespace localagent
