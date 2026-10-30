#pragma once

#include <chrono>
#include <cstddef>
#include <string>

namespace localagent {

struct ResourceUsage {
  double cpu_seconds{0.0};
  std::size_t peak_rss_bytes{0};
  std::chrono::seconds elapsed{0};
};

class ResourceMonitor {
public:
  ResourceMonitor();

  [[nodiscard]] ResourceUsage sample() const;

private:
  std::chrono::steady_clock::time_point started_at_;
};

[[nodiscard]] std::string format_resource_usage(const ResourceUsage& usage);

}  // namespace localagent
