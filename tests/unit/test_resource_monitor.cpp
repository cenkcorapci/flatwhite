#include "localagent/cli/resource_monitor.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace localagent;

TEST_CASE("Resource usage formatter renders compact process metrics", "[resource_monitor]") {
  const ResourceUsage usage{
      .cpu_seconds = 1.2,
      .peak_rss_bytes = 12 * 1024 * 1024,
      .elapsed = std::chrono::seconds{65},
  };

  CHECK(format_resource_usage(usage) ==
        "resources cpu 1.2s | peak rss 12.0 MiB | elapsed 1:05");
}

TEST_CASE("Resource monitor samples non-negative process metrics", "[resource_monitor]") {
  const ResourceMonitor monitor;
  const auto usage = monitor.sample();

  CHECK(usage.cpu_seconds >= 0.0);
  CHECK(usage.elapsed >= std::chrono::seconds::zero());
}
