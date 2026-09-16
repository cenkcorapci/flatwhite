#include "localagent/process/process.hpp"

#include <array>
#include <chrono>
#include <climits>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <map>
#include <optional>
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

extern char** environ;

namespace localagent {
namespace {

constexpr int kPollTimeoutMs = 50;
constexpr std::size_t kReadChunkSize = 4096;

struct EnvBlock {
  std::vector<std::string> storage;
  std::vector<char*> pointers;
};

[[nodiscard]] std::vector<char*> build_argv(const std::vector<std::string>& argv) {
  std::vector<char*> result;
  result.reserve(argv.size() + 1);
  for (const auto& arg : argv) {
    result.push_back(const_cast<char*>(arg.c_str()));
  }
  result.push_back(nullptr);
  return result;
}

[[nodiscard]] EnvBlock build_env_block(const std::map<std::string, std::string>& overrides) {
  std::map<std::string, std::string> merged;
  if (environ != nullptr) {
    for (char** env = environ; *env != nullptr; ++env) {
      const std::string entry = *env;
      const auto eq = entry.find('=');
      if (eq == std::string::npos) {
        continue;
      }
      merged.emplace(entry.substr(0, eq), entry.substr(eq + 1));
    }
  }
  for (const auto& [key, value] : overrides) {
    merged[key] = value;
  }

  EnvBlock block;
  block.storage.reserve(merged.size());
  for (const auto& [key, value] : merged) {
    block.storage.push_back(key + '=' + value);
  }
  block.pointers.reserve(block.storage.size() + 1);
  for (auto& entry : block.storage) {
    block.pointers.push_back(entry.data());
  }
  block.pointers.push_back(nullptr);
  return block;
}

[[nodiscard]] std::optional<std::string> resolve_executable(const std::string& file) {
  if (file.empty()) {
    return std::nullopt;
  }
  if (file.find('/') != std::string::npos) {
    return file;
  }

  const char* path_env = std::getenv("PATH");
  if (path_env == nullptr) {
    return std::nullopt;
  }

  std::string path = path_env;
  std::size_t start = 0;
  while (start <= path.size()) {
    const auto end = path.find(':', start);
    const std::string dir = path.substr(start, end - start);
    if (!dir.empty()) {
      const std::string candidate = dir + "/" + file;
      if (::access(candidate.c_str(), X_OK) == 0) {
        return candidate;
      }
    }
    if (end == std::string::npos) {
      break;
    }
    start = end + 1;
  }
  return std::nullopt;
}

void close_fd(int fd) {
  if (fd >= 0) {
    ::close(fd);
  }
}

void set_nonblocking(int fd) {
  const int flags = ::fcntl(fd, F_GETFL, 0);
  if (flags >= 0) {
    ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
  }
}

void kill_process_group(pid_t pid) {
  if (pid <= 0) {
    return;
  }
  const pid_t pgid = ::getpgid(pid);
  const pid_t my_pgid = ::getpgrp();
  if (pgid > 0 && pgid != my_pgid) {
    ::kill(-pgid, SIGTERM);
    ::usleep(50'000);
    ::kill(-pgid, SIGKILL);
    return;
  }
  ::kill(pid, SIGTERM);
  ::usleep(50'000);
  ::kill(pid, SIGKILL);
}

[[nodiscard]] bool read_available(int fd, std::string& out) {
  std::array<char, kReadChunkSize> buffer{};
  while (true) {
    const ssize_t n = ::read(fd, buffer.data(), buffer.size());
    if (n > 0) {
      out.append(buffer.data(), static_cast<std::size_t>(n));
      continue;
    }
    if (n == 0) {
      return true;
    }
    if (errno == EINTR) {
      continue;
    }
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return true;
    }
    return false;
  }
}

struct PipePair {
  int read_fd{-1};
  int write_fd{-1};
};

[[nodiscard]] std::optional<PipePair> make_pipe() {
  int fds[2]{-1, -1};
  if (::pipe(fds) != 0) {
    return std::nullopt;
  }
  return PipePair{.read_fd = fds[0], .write_fd = fds[1]};
}

[[nodiscard]] ProcessResult make_error_result(std::string stderr_msg) {
  ProcessResult result;
  result.exit_code = -1;
  result.stderr_str = std::move(stderr_msg);
  return result;
}

void drain_remaining(int stdout_fd, int stderr_fd, ProcessResult& result) {
  if (stdout_fd >= 0) {
    static_cast<void>(read_available(stdout_fd, result.stdout_str));
  }
  if (stderr_fd >= 0) {
    static_cast<void>(read_available(stderr_fd, result.stderr_str));
  }
}

}  // namespace

ProcessResult run_process(const ProcessSpec& spec, CancellationToken token) {
  if (spec.argv.empty()) {
    return make_error_result("process argv is empty");
  }

  if (token.is_cancelled()) {
    ProcessResult result;
    result.cancelled = true;
    return result;
  }

  if (spec.use_pty) {
    return make_error_result("PTY mode is not implemented yet");
  }

  const auto executable = resolve_executable(spec.argv[0]);
  if (!executable) {
    return make_error_result("executable not found: " + spec.argv[0]);
  }

  auto stdout_pipe = make_pipe();
  auto stderr_pipe = make_pipe();
  if (!stdout_pipe || !stderr_pipe) {
    return make_error_result("failed to create stdout/stderr pipes");
  }

  auto argv = build_argv(spec.argv);
  auto env_block = build_env_block(spec.env);

  const pid_t pid = ::fork();
  if (pid < 0) {
    close_fd(stdout_pipe->read_fd);
    close_fd(stdout_pipe->write_fd);
    close_fd(stderr_pipe->read_fd);
    close_fd(stderr_pipe->write_fd);
    return make_error_result(std::string("fork failed: ") + std::strerror(errno));
  }

  if (pid == 0) {
    ::setsid();
    ::setpgid(0, 0);

    if (spec.cwd && !spec.cwd->empty()) {
      if (::chdir(spec.cwd->c_str()) != 0) {
        _exit(126);
      }
    }

    close_fd(stdout_pipe->read_fd);
    close_fd(stderr_pipe->read_fd);

    if (::dup2(stdout_pipe->write_fd, STDOUT_FILENO) < 0) {
      _exit(126);
    }
    if (::dup2(stderr_pipe->write_fd, STDERR_FILENO) < 0) {
      _exit(126);
    }

    close_fd(stdout_pipe->write_fd);
    close_fd(stderr_pipe->write_fd);

    ::execve(executable->c_str(), argv.data(), env_block.pointers.data());
    _exit(127);
  }

  ::setpgid(pid, pid);

  close_fd(stdout_pipe->write_fd);
  close_fd(stderr_pipe->write_fd);

  set_nonblocking(stdout_pipe->read_fd);
  set_nonblocking(stderr_pipe->read_fd);

  ProcessResult result;
  bool stdout_open = true;
  bool stderr_open = true;
  bool child_exited = false;

  const auto deadline = spec.timeout_ms > 0
                            ? std::optional<std::chrono::steady_clock::time_point>(
                                  std::chrono::steady_clock::now() +
                                  std::chrono::milliseconds(spec.timeout_ms))
                            : std::nullopt;

  while (!child_exited || stdout_open || stderr_open) {
    if (token.is_cancelled()) {
      result.cancelled = true;
      kill_process_group(pid);
      break;
    }

    if (deadline && std::chrono::steady_clock::now() >= *deadline) {
      result.timed_out = true;
      kill_process_group(pid);
      break;
    }

    std::array<pollfd, 2> fds{};
    int nfds = 0;
    if (stdout_open) {
      fds[nfds++] = pollfd{.fd = stdout_pipe->read_fd, .events = POLLIN, .revents = 0};
    }
    if (stderr_open) {
      fds[nfds++] = pollfd{.fd = stderr_pipe->read_fd, .events = POLLIN, .revents = 0};
    }

    if (nfds > 0) {
      int poll_timeout = kPollTimeoutMs;
      if (deadline) {
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
            *deadline - std::chrono::steady_clock::now());
        poll_timeout = static_cast<int>(std::max<int64_t>(0, remaining.count()));
        poll_timeout = std::min(poll_timeout, kPollTimeoutMs);
      }

      const int poll_rc = ::poll(fds.data(), nfds, poll_timeout);
      if (poll_rc < 0 && errno != EINTR) {
        result.stderr_str += std::string("poll failed: ") + std::strerror(errno);
        kill_process_group(pid);
        break;
      }

      int idx = 0;
      if (stdout_open) {
        const auto& pfd = fds[idx++];
        if (pfd.revents & POLLIN) {
          static_cast<void>(read_available(stdout_pipe->read_fd, result.stdout_str));
        }
        if (pfd.revents & (POLLHUP | POLLERR | POLLNVAL)) {
          static_cast<void>(read_available(stdout_pipe->read_fd, result.stdout_str));
          stdout_open = false;
        }
      }
      if (stderr_open) {
        const auto& pfd = fds[idx++];
        if (pfd.revents & POLLIN) {
          static_cast<void>(read_available(stderr_pipe->read_fd, result.stderr_str));
        }
        if (pfd.revents & (POLLHUP | POLLERR | POLLNVAL)) {
          static_cast<void>(read_available(stderr_pipe->read_fd, result.stderr_str));
          stderr_open = false;
        }
      }
    } else {
      ::usleep(10'000);
    }

    int status = 0;
    const pid_t waited = ::waitpid(pid, &status, WNOHANG);
    if (waited == pid) {
      child_exited = true;
      if (WIFEXITED(status)) {
        result.exit_code = WEXITSTATUS(status);
      } else if (WIFSIGNALED(status)) {
        result.exit_code = 128 + WTERMSIG(status);
      }
    }
  }

  drain_remaining(stdout_pipe->read_fd, stderr_pipe->read_fd, result);
  close_fd(stdout_pipe->read_fd);
  close_fd(stderr_pipe->read_fd);

  if (result.exit_code == -1) {
    int status = 0;
    const pid_t waited = ::waitpid(pid, &status, 0);
    if (waited == pid) {
      if (WIFEXITED(status)) {
        result.exit_code = WEXITSTATUS(status);
      } else if (WIFSIGNALED(status)) {
        result.exit_code = 128 + WTERMSIG(status);
      }
    }
  }

  return result;
}

ProcessResult run_process(const std::vector<std::string>& argv,
                          const std::filesystem::path& cwd,
                          const CancellationToken& token,
                          std::chrono::seconds timeout) {
  ProcessSpec spec;
  spec.argv = argv;
  spec.cwd = cwd.string();
  const auto timeout_ms = std::chrono::duration_cast<std::chrono::milliseconds>(timeout);
  spec.timeout_ms = static_cast<std::uint32_t>(
      std::min<std::int64_t>(timeout_ms.count(), UINT32_MAX));
  return run_process(spec, token);
}

}  // namespace localagent
