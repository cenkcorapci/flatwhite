#pragma once

#include <atomic>
#include <memory>
#include <stdexcept>

namespace localagent {

class CancellationRequested : public std::runtime_error {
public:
  CancellationRequested() : std::runtime_error("operation cancelled") {}
};

class CancellationToken {
public:
  CancellationToken() : state_(std::make_shared<std::atomic<bool>>(false)) {}

  void cancel() noexcept { state_->store(true, std::memory_order_release); }
  [[nodiscard]] bool is_cancelled() const noexcept {
    return state_->load(std::memory_order_acquire);
  }
  void throw_if_cancelled() const {
    if (is_cancelled()) {
      throw CancellationRequested{};
    }
  }

private:
  std::shared_ptr<std::atomic<bool>> state_;
};

}  // namespace localagent
