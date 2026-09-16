#include "localagent/tools/filesystem/patch_engine.hpp"

#include <fstream>
#include <sstream>

namespace localagent::patch {

bool atomic_write(const std::filesystem::path& path, std::string_view content) {
  const auto tmp = path.string() + ".localagent.tmp";
  {
    std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
    if (!out) {
      return false;
    }
    out.write(content.data(), static_cast<std::streamsize>(content.size()));
    if (!out) {
      std::filesystem::remove(tmp);
      return false;
    }
  }
  std::error_code ec;
  std::filesystem::rename(tmp, path, ec);
  if (ec) {
    std::filesystem::remove(tmp);
    return false;
  }
  return true;
}

PatchResult exact_replace(const std::filesystem::path& path, std::string_view old_text,
                          std::string_view new_text) {
  PatchResult result;
  if (!std::filesystem::exists(path)) {
    result.message = "file not found";
    return result;
  }
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    result.message = "failed to read file";
    return result;
  }
  std::ostringstream oss;
  oss << in.rdbuf();
  const auto content = oss.str();
  const auto first = content.find(old_text);
  if (first == std::string::npos) {
    result.message = "old text not found";
    return result;
  }
  const auto second = content.find(old_text, first + old_text.size());
  if (second != std::string::npos) {
    result.message = "old text is not unique";
    return result;
  }
  std::string updated = content;
  updated.replace(first, old_text.size(), new_text);
  if (!atomic_write(path, updated)) {
    result.message = "atomic write failed";
    return result;
  }
  result.success = true;
  result.message = "replaced";
  return result;
}

PatchResult create_file(const std::filesystem::path& path, std::string_view content,
                        bool overwrite) {
  PatchResult result;
  if (std::filesystem::exists(path) && !overwrite) {
    result.message = "file already exists";
    return result;
  }
  std::filesystem::create_directories(path.parent_path());
  if (!atomic_write(path, content)) {
    result.message = "atomic write failed";
    return result;
  }
  result.success = true;
  result.message = "created";
  return result;
}

PatchResult apply_unified_diff(const std::filesystem::path& path, std::string_view diff_text) {
  PatchResult result;
  if (!std::filesystem::exists(path)) {
    result.message = "file not found";
    return result;
  }

  std::istringstream diff{std::string{diff_text}};
  std::string line;
  std::string old_chunk;
  std::string new_chunk;
  bool in_hunk = false;
  while (std::getline(diff, line)) {
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    if (line.rfind("@@", 0) == 0) {
      in_hunk = true;
      continue;
    }
    if (!in_hunk) {
      continue;
    }
    if (line.empty()) {
      continue;
    }
    const char prefix = line[0];
    const std::string payload = line.size() > 1 ? line.substr(1) : std::string{};
    if (prefix == ' ') {
      old_chunk += payload;
      old_chunk.push_back('\n');
      new_chunk += payload;
      new_chunk.push_back('\n');
    } else if (prefix == '-') {
      old_chunk += payload;
      old_chunk.push_back('\n');
    } else if (prefix == '+') {
      new_chunk += payload;
      new_chunk.push_back('\n');
    }
  }

  if (old_chunk.empty() && new_chunk.empty()) {
    result.message = "no applicable hunks";
    return result;
  }

  std::ifstream in(path, std::ios::binary);
  std::ostringstream oss;
  oss << in.rdbuf();
  auto content = oss.str();
  const auto pos = content.find(old_chunk);
  if (pos == std::string::npos) {
    result.message = "hunk context not found";
    return result;
  }
  content.replace(pos, old_chunk.size(), new_chunk);
  if (!atomic_write(path, content)) {
    result.message = "atomic write failed";
    return result;
  }
  result.success = true;
  result.message = "diff applied";
  return result;
}

}  // namespace localagent::patch
