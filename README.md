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

`ctest --test-dir build --output-on-failure` runs unit tests and CLI integration
tests. The integration coverage verifies help output, deterministic fake-model
JSON output, and session persistence without requiring a model service.

## Documentation

- [Getting started](docs/getting-started.md): prerequisites, build steps,
  offline validation, and Ollama usage.
- [Configuration and sessions](docs/configuration.md): configuration precedence,
  supported settings, permissions, environment overrides, and persistent data.

## License

See `LICENSE`.
