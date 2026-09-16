#include "localagent/context/rules_loader.hpp"

#include <array>
#include <cctype>
#include <fstream>
#include <regex>
#include <sstream>

namespace localagent {

RulesLoader::RulesLoader(std::filesystem::path workspace_root)
    : workspace_root_(std::move(workspace_root)) {}

namespace {

std::string read_file_text(const std::filesystem::path& path) {
  std::ifstream in(path);
  if (!in) {
    return {};
  }
  std::ostringstream oss;
  oss << in.rdbuf();
  return oss.str();
}

std::vector<std::string> parse_frontmatter_globs(std::string_view content) {
  if (!content.starts_with("---")) {
    return {};
  }
  const auto end = content.find("\n---", 3);
  if (end == std::string_view::npos) {
    return {};
  }
  const auto header = content.substr(3, end - 3);
  std::vector<std::string> globs;
  std::istringstream iss{std::string(header)};
  std::string line;
  while (std::getline(iss, line)) {
    const auto pos = line.find("globs:");
    if (pos != std::string::npos) {
      auto value = line.substr(pos + 6);
      while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) {
        value.erase(value.begin());
      }
      if (!value.empty()) {
        globs.push_back(value);
      }
    }
  }
  return globs;
}

}  // namespace

bool RulesLoader::glob_matches(std::string_view pattern, std::string_view path) {
  auto to_regex = [](std::string_view glob) {
    std::string regex_pattern;
    for (char c : glob) {
      switch (c) {
        case '*':
          regex_pattern += ".*";
          break;
        case '?':
          regex_pattern += '.';
          break;
        default:
          if (strchr("+.()[]{}^$|\\", c)) {
            regex_pattern.push_back('\\');
          }
          regex_pattern.push_back(c);
          break;
      }
    }
    return regex_pattern;
  };

  const std::regex full_re(to_regex(pattern));
  if (std::regex_match(std::string(path), full_re)) {
    return true;
  }
  const auto slash = path.find_last_of('/');
  const auto filename = slash == std::string_view::npos ? path : path.substr(slash + 1);
  const std::regex name_re(to_regex(pattern));
  return std::regex_match(std::string(filename), name_re);
}

std::vector<RuleFile> RulesLoader::load_directory_rules(
    const std::filesystem::path& dir) const {
  std::vector<RuleFile> out;
  std::error_code ec;
  if (!std::filesystem::exists(dir, ec)) {
    return out;
  }
  for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
    if (!entry.is_regular_file()) {
      continue;
    }
    const auto content = read_file_text(entry.path());
    RuleFile rf{entry.path(), content, parse_frontmatter_globs(content)};
    if (rf.globs.empty()) {
      rf.globs.push_back("**");
    }
    out.push_back(std::move(rf));
  }
  return out;
}

std::vector<RuleFile> RulesLoader::load_all() const {
  std::vector<RuleFile> out;
  const std::array files = {"AGENTS.md", "CLAUDE.md"};
  for (const auto* name : files) {
    const auto path = workspace_root_ / name;
    if (std::filesystem::exists(path)) {
      out.push_back(RuleFile{path, read_file_text(path), {"**"}});
    }
  }
  const std::array dirs = {".agent/rules", ".cursor/rules"};
  for (const auto* dir : dirs) {
    auto rules = load_directory_rules(workspace_root_ / dir);
    out.insert(out.end(), std::make_move_iterator(rules.begin()),
               std::make_move_iterator(rules.end()));
  }
  return out;
}

std::vector<RuleFile> RulesLoader::match_for_path(std::string_view relative_path) const {
  const auto all = load_all();
  std::vector<RuleFile> matched;
  for (const auto& rule : all) {
    for (const auto& glob : rule.globs) {
      if (glob_matches(glob, relative_path)) {
        matched.push_back(rule);
        break;
      }
    }
  }
  return matched;
}

}  // namespace localagent
