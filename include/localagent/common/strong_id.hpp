#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <utility>

namespace localagent {

template <typename Tag>
class StrongId {
public:
  StrongId() = default;
  explicit StrongId(std::string value) : value_(std::move(value)) {}

  [[nodiscard]] const std::string& str() const noexcept { return value_; }
  [[nodiscard]] bool empty() const noexcept { return value_.empty(); }

  friend bool operator==(const StrongId& a, const StrongId& b) noexcept {
    return a.value_ == b.value_;
  }
  friend bool operator!=(const StrongId& a, const StrongId& b) noexcept {
    return !(a == b);
  }
  friend bool operator<(const StrongId& a, const StrongId& b) noexcept {
    return a.value_ < b.value_;
  }

private:
  std::string value_;
};

struct SessionTag {};
struct RunTag {};
struct ToolCallTag {};
struct ModelCallTag {};
struct JobTag {};
struct MessageTag {};
struct EventTag {};
struct ContextTag {};
struct StepTag {};

using SessionId = StrongId<SessionTag>;
using RunId = StrongId<RunTag>;
using ToolCallId = StrongId<ToolCallTag>;
using ModelCallId = StrongId<ModelCallTag>;
using JobId = StrongId<JobTag>;
using MessageId = StrongId<MessageTag>;
using EventId = StrongId<EventTag>;
using ContextId = StrongId<ContextTag>;
using StepId = StrongId<StepTag>;

[[nodiscard]] std::string make_uuid();

}  // namespace localagent

namespace std {
template <typename Tag>
struct hash<localagent::StrongId<Tag>> {
  size_t operator()(const localagent::StrongId<Tag>& id) const noexcept {
    return hash<string>{}(id.str());
  }
};
}  // namespace std
