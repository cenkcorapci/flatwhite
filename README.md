# localagent

## Install

```bash
curl -fsSL https://raw.githubusercontent.com/cenkcorapci/flatwhite/main/install.sh | sh
```

The installer installs the required packages on Ubuntu/Debian (with `sudo` when
needed) or with Homebrew on macOS, then clones, builds, installs to
`~/.localagent`, and runs an offline fake-backend verification (no model
download). On unsupported package managers, it prints the required dependencies
instead of changing the system. It refuses to overwrite an existing non-git
checkout.
Set `LOCALAGENT_INSTALL_DIR` to choose another prefix; set `LOCALAGENT_RUN=1`
to run `--help` after installation.

Local-first C++23 AI agent CLI. The LLM is a probabilistic component inside a deterministic execution harness (permissions, budgets, tools, verification, persistence).

## Status

MVP covering architecture phases 0–3 plus basic phase 4:

- CLI modes: `agent`, `plan`, `ask`, `debug`, `review`, `chat`, `data`, `research`
- Ollama HTTP backend + fake model for offline/CI
- Tool runtime: filesystem search/edit, patch, shell, git, and web search
- Unrestricted development-tool permissions by default, with deterministic risk metadata
- Agent state machine, budgets, planning, verification hooks
- SQLite + JSONL session/event persistence
- Context builder, rules loader (`AGENTS.md`, `.agent/rules/`), project detection

## Quick start

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Dependencies: C++23 compiler, CMake ≥ 3.24, Ninja, libcurl, SQLite3. Catch2, nlohmann/json, and spdlog are fetched via CMake FetchContent.

Run the offline smoke path first; it uses the deterministic fake backend and
requires neither Ollama nor a downloaded model:

```bash
LOCALAGENT_FAKE_MODEL=1 ./build/localagent -p "hello" --output-format text
```

For model-backed use, ensure Ollama is running locally and then select a mode:

```bash
./build/localagent chat -p "summarize this repository"
./build/localagent agent -p "fix the failing test"
```

## CLI usage

```bash
# Inspect supported modes and flags
./build/localagent --help
./build/localagent --version

# JSON output for automation
./build/localagent -p "explain main.cpp" --output-format json

# Stored sessions
./build/localagent sessions list
```

Use `-p` / `--prompt` or a positional prompt. The CLI provides `chat`, `agent`,
`plan`, `ask`, `debug`, `review`, `data`, and `research` modes. In CI and
offline checks, set `LOCALAGENT_FAKE_MODEL=1` or pass `--fake-model`.

## Tests

Run focused unit, integration, or all tests with:

```bash
./build/localagent_tests
ctest --test-dir build -R '^integration_' --output-on-failure
ctest --test-dir build --output-on-failure
```

Tests use the fake backend and injected web-search transport; they require
neither Ollama, a downloaded model, nor public network access.
When Docker is available, the integration command also builds a clean Ubuntu
container and pipes the installer through curl against a local file URL.

## Development tools and web search

Every registered development tool is available without approval by default:
`read_file`, `write_file`, `list_directory`, `glob`, `grep`, `shell`,
`git_status`, `git_diff`, `git_log`, and `web_search`. Risk classification is
still recorded for observability. This is intentionally an operational
trust boundary: run the agent only in workspaces and environments you trust.

`web_search` uses an HTTP provider endpoint, defaults to DuckDuckGo's HTML
endpoint, and does not require an SDK or credentials. Configure a different
provider with `LOCALAGENT_WEB_SEARCH_ENDPOINT` or `[web_search].endpoint`;
include `{query}` where its percent-encoded query belongs.

## Documentation

- [Getting started](docs/getting-started.md): prerequisites, build steps,
  offline validation, and Ollama usage.
- [Configuration and sessions](docs/configuration.md): configuration precedence,
  supported settings, permissions, web search, environment overrides, and persistent data.

## License

See `LICENSE`.
