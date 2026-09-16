#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace localagent::patch {

struct PatchResult {
  bool success{false};
  std::string message;
};

[[nodiscard]] PatchResult exact_replace(const std::filesystem::path& path,
                                        std::string_view old_text, std::string_view new_text);

[[nodiscard]] PatchResult create_file(const std::filesystem::path& path,
                                      std::string_view content, bool overwrite = false);

[[nodiscard]] PatchResult apply_unified_diff(const std::filesystem::path& path,
                                               std::string_view diff_text);

[[nodiscard]] bool atomic_write(const std::filesystem::path& path, std::string_view content);

}  // namespace localagent::patch
