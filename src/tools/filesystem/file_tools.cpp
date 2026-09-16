#include "localagent/tools/filesystem/file_tools.hpp"

#include "localagent/common/json_util.hpp"
#include "localagent/tools/filesystem/patch_engine.hpp"

#include <fstream>
#include <regex>
#include <sstream>

namespace localagent {

bool path_within_workspace(const std::filesystem::path& workspace,
                           const std::filesystem::path& candidate) {
  std::error_code ec;
  const auto root = std::filesystem::weakly_canonical(workspace, ec);
  const auto path = std::filesystem::weakly_canonical(candidate, ec);
  if (ec) {
    return false;
  }
  auto rel = std::filesystem::relative(path, root, ec);
  if (ec || rel.empty()) {
    return path == root;
  }
  if (rel.string().starts_with("..")) {
    return false;
  }
  return true;
}

std::filesystem::path resolve_workspace_path(const ToolContext& ctx,
                                             const std::string& user_path,
                                             bool allow_outside) {
  std::filesystem::path p(user_path);
  if (!p.is_absolute()) {
    p = ctx.workspace_root / p;
  }
  std::error_code ec;
  p = std::filesystem::weakly_canonical(p, ec);
  if (ec) {
    p = std::filesystem::absolute(user_path, ec);
    if (!p.is_absolute()) {
      p = ctx.workspace_root / user_path;
    }
  }
  if (!allow_outside && !path_within_workspace(ctx.workspace_root, p)) {
    if (!ctx.permissions ||
        !ctx.permissions->allows(
            ctx.permissions->check(RiskLevel::OutsideWorkspaceWrite, p))) {
      throw std::runtime_error("path escapes workspace: " + user_path);
    }
  }
  return p;
}

namespace {

ToolResult ok(std::string content, nlohmann::json meta = {}) {
  return ToolResult{true, std::move(content), std::move(meta), std::nullopt};
}

ToolResult fail(std::string message) {
  return ToolResult{false, {}, {}, make_error(ErrorCategory::Tool, "tool_error", std::move(message))};
}

bool glob_match(std::string_view pattern, std::string_view text) {
  std::string regex_pattern;
  regex_pattern.reserve(pattern.size() * 2);
  for (char c : pattern) {
    switch (c) {
      case '*':
        regex_pattern += ".*";
        break;
      case '?':
        regex_pattern += '.';
        break;
      case '.':
      case '+':
      case '(':
      case ')':
      case '[':
      case ']':
      case '{':
      case '}':
      case '^':
      case '$':
      case '|':
      case '\\':
        regex_pattern.push_back('\\');
        regex_pattern.push_back(c);
        break;
      default:
        regex_pattern.push_back(c);
        break;
    }
  }
  const std::regex re(regex_pattern, std::regex::ECMAScript);
  return std::regex_match(std::string(text), re);
}

void glob_recursive(const std::filesystem::path& root, const std::filesystem::path& dir,
                    std::string_view pattern, std::vector<std::string>& out) {
  std::error_code ec;
  for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
    const auto rel = std::filesystem::relative(entry.path(), root, ec);
    if (ec) {
      continue;
    }
    const auto rel_str = rel.generic_string();
    if (entry.is_regular_file() && glob_match(pattern, rel_str)) {
      out.push_back(rel_str);
    }
    if (entry.is_directory()) {
      glob_recursive(root, entry.path(), pattern, out);
    }
  }
}

}  // namespace

ToolDescriptor ReadFileTool::descriptor() const {
  return ToolDescriptor{
      "read_file",
      "Read a UTF-8 text file within the workspace.",
      {{"type", "object"},
       {"properties",
        {{"path", {{"type", "string"}, {"description", "Relative or absolute file path"}}}}},
       {"required", nlohmann::json::array({"path"})}}};
}

ToolResult ReadFileTool::execute(const ToolRequest& request, const ToolContext& ctx) const {
  const auto path_str = json_string(request.arguments, "path");
  if (!path_str) {
    return fail("missing path");
  }
  ctx.cancellation.throw_if_cancelled();
  try {
    const auto path = resolve_workspace_path(ctx, *path_str, false);
    if (ctx.permissions) {
      const auto decision = ctx.permissions->evaluate(RiskLevel::Read, path);
      if (!ctx.permissions->allows(decision.policy)) {
        return fail("permission denied: " + decision.reason);
      }
    }
    std::ifstream in(path, std::ios::binary);
    if (!in) {
      return fail("unable to open file");
    }
    std::ostringstream oss;
    oss << in.rdbuf();
    return ok(oss.str(), {{"path", path.generic_string()}});
  } catch (const std::exception& ex) {
    return fail(ex.what());
  }
}

ToolDescriptor WriteFileTool::descriptor() const {
  return ToolDescriptor{
      "write_file",
      "Write content to a file using atomic replace.",
      {{"type", "object"},
       {"properties",
        {{"path", {{"type", "string"}}}, {"content", {{"type", "string"}}}}},
       {"required", nlohmann::json::array({"path", "content"})}}};
}

ToolResult WriteFileTool::execute(const ToolRequest& request, const ToolContext& ctx) const {
  const auto path_str = json_string(request.arguments, "path");
  const auto content = json_string(request.arguments, "content");
  if (!path_str || !content) {
    return fail("missing path or content");
  }
  ctx.cancellation.throw_if_cancelled();
  try {
    const auto path = resolve_workspace_path(ctx, *path_str, false);
    if (ctx.permissions) {
      const auto decision = ctx.permissions->evaluate(RiskLevel::WorkspaceWrite, path);
      if (!ctx.permissions->allows(decision.policy)) {
        return fail("permission denied: " + decision.reason);
      }
    }
    std::filesystem::create_directories(path.parent_path());
    if (!patch::atomic_write(path, *content)) {
      return fail("write failed");
    }
    return ok("written", {{"path", path.generic_string()}});
  } catch (const std::exception& ex) {
    return fail(ex.what());
  }
}

ToolDescriptor ListDirectoryTool::descriptor() const {
  return ToolDescriptor{
      "list_directory",
      "List entries in a workspace directory.",
      {{"type", "object"},
       {"properties", {{"path", {{"type", "string"}, {"default", "."}}}}},
       {"required", nlohmann::json::array()}}};
}

ToolResult ListDirectoryTool::execute(const ToolRequest& request, const ToolContext& ctx) const {
  const auto path_str = json_string(request.arguments, "path").value_or(".");
  ctx.cancellation.throw_if_cancelled();
  try {
    const auto path = resolve_workspace_path(ctx, path_str, false);
    if (!std::filesystem::is_directory(path)) {
      return fail("not a directory");
    }
    nlohmann::json entries = nlohmann::json::array();
    for (const auto& entry : std::filesystem::directory_iterator(path)) {
      entries.push_back({{"name", entry.path().filename().string()},
                         {"type", entry.is_directory() ? "directory" : "file"}});
    }
    return ok(entries.dump(2));
  } catch (const std::exception& ex) {
    return fail(ex.what());
  }
}

ToolDescriptor GlobTool::descriptor() const {
  return ToolDescriptor{
      "glob",
      "Find files matching a glob pattern under the workspace.",
      {{"type", "object"},
       {"properties", {{"pattern", {{"type", "string"}}}}},
       {"required", nlohmann::json::array({"pattern"})}}};
}

ToolResult GlobTool::execute(const ToolRequest& request, const ToolContext& ctx) const {
  const auto pattern = json_string(request.arguments, "pattern");
  if (!pattern) {
    return fail("missing pattern");
  }
  ctx.cancellation.throw_if_cancelled();
  std::vector<std::string> matches;
  glob_recursive(ctx.workspace_root, ctx.workspace_root, *pattern, matches);
  std::sort(matches.begin(), matches.end());
  nlohmann::json arr = matches;
  return ok(arr.dump(2));
}

ToolDescriptor GrepTool::descriptor() const {
  return ToolDescriptor{
      "grep",
      "Search for a regex pattern in workspace files.",
      {{"type", "object"},
       {"properties",
        {{"pattern", {{"type", "string"}}}, {"path", {{"type", "string"}, {"default", "."}}}}},
       {"required", nlohmann::json::array({"pattern"})}}};
}

ToolResult GrepTool::execute(const ToolRequest& request, const ToolContext& ctx) const {
  const auto pattern = json_string(request.arguments, "pattern");
  const auto path_str = json_string(request.arguments, "path").value_or(".");
  if (!pattern) {
    return fail("missing pattern");
  }
  ctx.cancellation.throw_if_cancelled();
  try {
    const auto root = resolve_workspace_path(ctx, path_str, false);
    const std::regex re(*pattern);
    nlohmann::json hits = nlohmann::json::array();
    const auto scan = [&](const std::filesystem::path& file) {
      std::ifstream in(file);
      if (!in) {
        return;
      }
      std::string line;
      int line_no = 0;
      while (std::getline(in, line)) {
        ++line_no;
        if (std::regex_search(line, re)) {
          hits.push_back({{"file", std::filesystem::relative(file, ctx.workspace_root).generic_string()},
                          {"line", line_no},
                          {"text", line}});
        }
      }
    };
    if (std::filesystem::is_regular_file(root)) {
      scan(root);
    } else {
      for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
        if (entry.is_regular_file()) {
          scan(entry.path());
        }
      }
    }
    return ok(hits.dump(2));
  } catch (const std::exception& ex) {
    return fail(ex.what());
  }
}

std::vector<std::unique_ptr<Tool>> make_file_tools() {
  std::vector<std::unique_ptr<Tool>> tools;
  tools.push_back(std::make_unique<ReadFileTool>());
  tools.push_back(std::make_unique<WriteFileTool>());
  tools.push_back(std::make_unique<ListDirectoryTool>());
  tools.push_back(std::make_unique<GlobTool>());
  tools.push_back(std::make_unique<GrepTool>());
  return tools;
}

}  // namespace localagent
