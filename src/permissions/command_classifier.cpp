#include "localagent/permissions/command_classifier.hpp"

#include <algorithm>
#include <cctype>

namespace localagent {

namespace {

std::string basename_of(const std::string& command) {
  const auto slash = command.find_last_of("/\\");
  if (slash == std::string::npos) {
    return command;
  }
  return command.substr(slash + 1);
}

void add_risk(CommandClassification& classification, RiskLevel risk) {
  classification.risks.push_back(risk);
  if (static_cast<int>(risk) > static_cast<int>(classification.primary_risk)) {
    classification.primary_risk = risk;
  }
}

bool argv_contains(const std::vector<std::string>& argv, std::string_view needle) {
  return std::ranges::any_of(argv, [&](const std::string& arg) { return arg == needle; });
}

}  // namespace

CommandClassification classify_command(const std::vector<std::string>& argv) {
  CommandClassification classification{
      .primary_risk = RiskLevel::Read,
      .summary = "unknown",
  };

  if (argv.empty()) {
    classification.summary = "empty";
    return classification;
  }

  const auto command = basename_of(argv.front());

  if (command == "git") {
    if (argv.size() >= 2) {
      const auto& subcommand = argv[1];
      if (subcommand == "status" || subcommand == "diff" || subcommand == "log" ||
          subcommand == "show" || subcommand == "branch") {
        add_risk(classification, RiskLevel::Read);
        classification.summary = "git read";
        return classification;
      }
      if (subcommand == "clone" || subcommand == "fetch" || subcommand == "pull") {
        add_risk(classification, RiskLevel::Network);
        classification.summary = "git network";
        return classification;
      }
    }
    add_risk(classification, RiskLevel::WorkspaceWrite);
    classification.summary = "git write";
    return classification;
  }

  if (command == "rm") {
    add_risk(classification, RiskLevel::Destructive);
    classification.summary = "remove";
    return classification;
  }

  if (command == "sudo" || command == "su" || command == "doas") {
    add_risk(classification, RiskLevel::Privilege);
    classification.summary = "privilege escalation";
    return classification;
  }

  if (command == "chmod" || command == "chown" || command == "chgrp") {
    add_risk(classification, RiskLevel::Privilege);
    classification.summary = "permission change";
    return classification;
  }

  if (command == "dd") {
    add_risk(classification, RiskLevel::Destructive);
    add_risk(classification, RiskLevel::Privilege);
    classification.summary = "disk write";
    return classification;
  }

  if (command == "curl" || command == "wget") {
    add_risk(classification, RiskLevel::Network);
    if (argv_contains(argv, "|") || argv_contains(argv, "sh") ||
        argv_contains(argv, "bash")) {
      add_risk(classification, RiskLevel::Destructive);
      classification.summary = "curl pipe shell";
    } else {
      classification.summary = "network fetch";
    }
    return classification;
  }

  if (command == "sh" || command == "bash" || command == "zsh") {
    add_risk(classification, RiskLevel::Destructive);
    classification.summary = "shell execution";
    return classification;
  }

  if (command == "pip" || command == "pip3" || command == "npm" || command == "yarn" ||
      command == "pnpm" || command == "brew" || command == "apt" || command == "apt-get" ||
      command == "uv") {
    add_risk(classification, RiskLevel::PackageInstall);
    classification.summary = "package install";
    return classification;
  }

  if (command == "cmake" || command == "make" || command == "ninja" || command == "ctest" ||
      command == "go" || command == "pytest" || command == "cargo") {
    add_risk(classification, RiskLevel::Read);
    classification.summary = "build/test";
    return classification;
  }

  classification.summary = command;
  add_risk(classification, RiskLevel::Read);
  return classification;
}

}  // namespace localagent
