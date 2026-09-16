#pragma once

#include "localagent/common/strong_id.hpp"
#include "localagent/persistence/sqlite_store.hpp"

#include <filesystem>
#include <memory>
#include <optional>
#include <string>

namespace localagent {

class SessionController {
public:
  explicit SessionController(std::filesystem::path data_dir);

  [[nodiscard]] SessionId create(std::string title);
  [[nodiscard]] std::optional<SessionId> resume(const std::optional<std::string>& id,
                                                bool latest_if_missing);
  [[nodiscard]] std::vector<SessionRecord> list() const;

private:
  std::unique_ptr<SqliteStore> store_;
};

}  // namespace localagent
