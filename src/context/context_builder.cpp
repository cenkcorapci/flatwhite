#include "localagent/context/context_builder.hpp"

#include "localagent/context/project_detector.hpp"

#include <fstream>
#include <sstream>

namespace localagent {

ContextBuilder::ContextBuilder(std::filesystem::path workspace_root)
    : workspace_root_(std::move(workspace_root)) {}

uint32_t ContextBuilder::estimate_tokens(std::string_view text) {
  return static_cast<uint32_t>((text.size() + 3) / 4);
}

std::string ContextBuilder::read_snippet(const std::filesystem::path& path,
                                         std::size_t max_chars) const {
  std::ifstream in(path);
  if (!in) {
    return {};
  }
  std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  if (content.size() > max_chars) {
    content.resize(max_chars);
    content += "\n... [truncated]";
  }
  return content;
}

BuiltContext ContextBuilder::build(const ContextBuildRequest& request) const {
  BuiltContext built;
  uint64_t budget = request.token_budget;

  auto add_item = [&](ContextItem item) {
    if (item.token_estimate > budget) {
      built.truncated = true;
      return false;
    }
    budget -= item.token_estimate;
    built.total_tokens += item.token_estimate;
    built.items.push_back(std::move(item));
    return true;
  };

  {
    ContextItem goal{ContextItemKind::Goal, "goal", request.goal,
                     estimate_tokens(request.goal)};
    add_item(std::move(goal));
  }

  RulesLoader rules_loader(workspace_root_);
  const auto rules = rules_loader.load_all();
  for (const auto& rule : rules) {
    ContextItem item{ContextItemKind::Rule, rule.path.filename().string(), rule.content,
                     estimate_tokens(rule.content)};
    if (!add_item(std::move(item))) {
      break;
    }
  }

  ProjectDetector detector(workspace_root_);
  const auto project = detector.detect();
  if (project.kind != ProjectKind::Unknown) {
    std::ostringstream oss;
    oss << "project: " << to_string(project.kind) << "\nbuild: ";
    for (const auto& cmd : project.build_commands) {
      oss << cmd << "; ";
    }
    oss << "\ntest: ";
    for (const auto& cmd : project.test_commands) {
      oss << cmd << "; ";
    }
    const auto text = oss.str();
    add_item(ContextItem{ContextItemKind::ProjectInfo, "project", text, estimate_tokens(text)});
  }

  for (const auto& rel : request.file_paths) {
    const auto path = workspace_root_ / rel;
    if (!std::filesystem::exists(path) || !std::filesystem::is_regular_file(path)) {
      continue;
    }
    const auto snippet = read_snippet(path);
    ContextItem item{ContextItemKind::FileSnippet, rel, snippet, estimate_tokens(snippet)};
    if (!add_item(std::move(item))) {
      break;
    }
  }

  for (const auto& [role, content] : request.conversation) {
    const std::string label = role;
    ContextItem item{ContextItemKind::Conversation, label, content, estimate_tokens(content)};
    if (!add_item(std::move(item))) {
      break;
    }
  }

  return built;
}

}  // namespace localagent
