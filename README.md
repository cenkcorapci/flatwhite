# localagent

Local-first C++23 AI agent CLI. The LLM is a probabilistic component inside a deterministic execution harness (permissions, budgets, tools, verification, persistence).

## Status

MVP covering architecture phases 0–3 plus basic phase 4:

- CLI modes: `agent`, `plan`, `ask`, `debug`, `review`, `chat`, `data`, `research`
- Ollama HTTP backend + fake model for offline/CI
- Tool runtime: filesystem, patch, shell, git
- Deterministic permissions and secret redaction
- Agent state machine, budgets, planning, verification hooks
- SQLite + JSONL session/event persistence
- Context builder, rules loader (`AGENTS.md`, `.agent/rules/`), project detection

## Build

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Dependencies: C++23 compiler, CMake ≥ 3.24, Ninja, libcurl, SQLite3. Catch2, nlohmann/json, and spdlog are fetched via CMake FetchContent.

## Usage

```bash
# Help / version
./build/localagent --help
./build/localagent --version

# Offline smoke (no Ollama required)
LOCALAGENT_FAKE_MODEL=1 ./build/localagent -p "hello" --output-format text

# Chat / agent (requires local Ollama by default)
./build/localagent chat -p "summarize this repository"
./build/localagent agent -p "fix the failing test"

# JSON output for automation
./build/localagent -p "explain main.cpp" --output-format json
```

Config precedence: defaults → `~/.config/localagent/config.toml` → `.agent/config.toml` → `LOCALAGENT_*` env → CLI flags.

## Tests

Unit tests live under `tests/unit/` and run in GitHub Actions on Ubuntu and macOS, including an ASan/UBSan job.

## License

See `LICENSE`.
