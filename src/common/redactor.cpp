#include "localagent/common/redactor.hpp"

#include <cctype>
#include <string>

namespace localagent {
namespace {

constexpr std::string_view kRedacted = "[REDACTED]";

bool starts_with_ci(std::string_view text, std::string_view prefix) {
  if (text.size() < prefix.size()) {
    return false;
  }
  for (size_t i = 0; i < prefix.size(); ++i) {
    if (std::tolower(static_cast<unsigned char>(text[i])) !=
        std::tolower(static_cast<unsigned char>(prefix[i]))) {
      return false;
    }
  }
  return true;
}

bool is_ident_start(char c) {
  return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
}

bool is_ident(char c) {
  return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

void redact_env_assignments(std::string& text) {
  std::string out;
  out.reserve(text.size());
  size_t i = 0;
  while (i < text.size()) {
    const size_t line_start = i;
    size_t line_end = text.find('\n', i);
    if (line_end == std::string::npos) {
      line_end = text.size();
    }

    size_t key_start = line_start;
    while (key_start < line_end && (text[key_start] == ' ' || text[key_start] == '\t')) {
      ++key_start;
    }

    size_t key_end = key_start;
    if (key_end < line_end && is_ident_start(text[key_end])) {
      ++key_end;
      while (key_end < line_end && is_ident(text[key_end])) {
        ++key_end;
      }
    }

    size_t eq = key_end;
    while (eq < line_end && (text[eq] == ' ' || text[eq] == '\t')) {
      ++eq;
    }

    if (key_end > key_start && eq < line_end && text[eq] == '=') {
      size_t value_start = eq + 1;
      while (value_start < line_end && (text[value_start] == ' ' || text[value_start] == '\t')) {
        ++value_start;
      }
      size_t value_end = value_start;
      while (value_end < line_end && text[value_end] != ' ' && text[value_end] != '#' &&
             text[value_end] != '\t') {
        ++value_end;
      }

      const std::string_view key(text.data() + key_start, key_end - key_start);
      const bool sensitive = starts_with_ci(key, "API") || starts_with_ci(key, "TOKEN") ||
                             starts_with_ci(key, "SECRET") || starts_with_ci(key, "PASSWORD") ||
                             starts_with_ci(key, "PASSWD") || key.find("KEY") != std::string_view::npos ||
                             key.find("TOKEN") != std::string_view::npos ||
                             key.find("SECRET") != std::string_view::npos;

      // Always redact non-trivial assignment values that look like secrets, and
      // specifically API_KEY style keys. Keep simple DEBUG=true style values.
      const std::string_view value(text.data() + value_start, value_end - value_start);
      const bool looks_simple_bool =
          value == "true" || value == "false" || value == "1" || value == "0";
      if (sensitive || (!looks_simple_bool && value.size() >= 8)) {
        out.append(text, line_start, value_start - line_start);
        out.append(kRedacted);
        out.append(text, value_end, line_end - value_end);
      } else {
        out.append(text, line_start, line_end - line_start);
      }
    } else {
      out.append(text, line_start, line_end - line_start);
    }

    if (line_end < text.size()) {
      out.push_back('\n');
      i = line_end + 1;
    } else {
      i = line_end;
    }
  }
  text = std::move(out);
}

void replace_all(std::string& text, std::string_view from, std::string_view to) {
  size_t pos = 0;
  while ((pos = text.find(from, pos)) != std::string::npos) {
    text.replace(pos, from.size(), to);
    pos += to.size();
  }
}

void redact_aws_keys(std::string& text) {
  size_t pos = 0;
  while ((pos = text.find("AKIA", pos)) != std::string::npos) {
    if (pos + 20 <= text.size()) {
      bool ok = true;
      for (size_t i = 4; i < 20; ++i) {
        const char c = text[pos + i];
        if (!(std::isupper(static_cast<unsigned char>(c)) ||
              std::isdigit(static_cast<unsigned char>(c)))) {
          ok = false;
          break;
        }
      }
      if (ok) {
        text.replace(pos, 20, kRedacted);
        pos += kRedacted.size();
        continue;
      }
    }
    pos += 4;
  }
}

void redact_prefixed_tokens(std::string& text, std::string_view prefix, size_t min_len) {
  size_t pos = 0;
  while ((pos = text.find(prefix, pos)) != std::string::npos) {
    size_t end = pos + prefix.size();
    while (end < text.size() &&
           (std::isalnum(static_cast<unsigned char>(text[end])) || text[end] == '_' ||
            text[end] == '-')) {
      ++end;
    }
    if (end - pos >= min_len) {
      text.replace(pos, end - pos, kRedacted);
      pos += kRedacted.size();
    } else {
      pos += prefix.size();
    }
  }
}

void redact_pem_blocks(std::string& text) {
  constexpr std::string_view begin_marker = "-----BEGIN ";
  constexpr std::string_view end_marker = "-----END ";
  size_t pos = 0;
  while ((pos = text.find(begin_marker, pos)) != std::string::npos) {
    const auto end_pos = text.find(end_marker, pos);
    if (end_pos == std::string::npos) {
      break;
    }
    const auto end_line = text.find('\n', end_pos);
    const auto erase_end = end_line == std::string::npos ? text.size() : end_line;
    text.replace(pos, erase_end - pos, kRedacted);
    pos += kRedacted.size();
  }
}

}  // namespace

std::string redact_secrets(std::string text) {
  redact_env_assignments(text);
  redact_aws_keys(text);
  redact_prefixed_tokens(text, "ghp_", 24);
  redact_prefixed_tokens(text, "glpat-", 24);
  redact_prefixed_tokens(text, "sk-", 24);
  redact_pem_blocks(text);

  // Generic key=secret patterns remaining in free text.
  for (const char* key : {"token=", "TOKEN=", "password=", "PASSWORD=", "secret=", "SECRET="}) {
    size_t pos = 0;
    const std::string_view marker(key);
    while ((pos = text.find(marker, pos)) != std::string::npos) {
      size_t value_start = pos + marker.size();
      size_t value_end = value_start;
      while (value_end < text.size() && !std::isspace(static_cast<unsigned char>(text[value_end]))) {
        ++value_end;
      }
      text.replace(value_start, value_end - value_start, kRedacted);
      pos = value_start + kRedacted.size();
    }
  }

  (void)replace_all;
  return text;
}

}  // namespace localagent
