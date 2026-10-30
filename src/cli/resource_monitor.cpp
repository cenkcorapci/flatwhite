#include "localagent/cli/resource_monitor.hpp"

#include <iomanip>
#include <sstream>

#if !defined(_WIN32)
#include <sys/resource.h>
#endif

namespace localagent {
namespace {

#if !defined(_WIN32)
double seconds_from_timeval(const timeval& value) {
  return static_cast<double>(value.tv_sec) + static_cast<double>(value.tv_usec) / 1'000'000.0;
}
#endif

std::string format_elapsed(std::chrono::seconds elapsed) {
  const auto total_seconds = elapsed.count();
  const auto minutes = total_seconds / 60;
  const auto seconds = total_seconds % 60;
  std::ostringstream output;
  output << minutes << ":" << std::setfill('0') << std::setw(2) << seconds;
  return output.str();
}

}  // namespace

ResourceMonitor::ResourceMonitor() : started_at_(std::chrono::steady_clock::now()) {}

ResourceUsage ResourceMonitor::sample() const {
  ResourceUsage usage;
  usage.elapsed = std::chrono::duration_cast<std::chrono::seconds>(
      std::chrono::steady_clock::now() - started_at_);

#if !defined(_WIN32)
  rusage resource_usage{};
  if (getrusage(RUSAGE_SELF, &resource_usage) == 0) {
    usage.cpu_seconds =
        seconds_from_timeval(resource_usage.ru_utime) + seconds_from_timeval(resource_usage.ru_stime);
#if defined(__APPLE__)
    usage.peak_rss_bytes = static_cast<std::size_t>(resource_usage.ru_maxrss);
#else
    usage.peak_rss_bytes = static_cast<std::size_t>(resource_usage.ru_maxrss) * 1024;
#endif
  }
#endif

  return usage;
}

std::string format_resource_usage(const ResourceUsage& usage) {
  std::ostringstream output;
  output << "resources cpu " << std::fixed << std::setprecision(1) << usage.cpu_seconds
         << "s | peak rss " << std::setprecision(1)
         << static_cast<double>(usage.peak_rss_bytes) / (1024.0 * 1024.0) << " MiB | elapsed "
         << format_elapsed(usage.elapsed);
  return output.str();
}

}  // namespace localagent
