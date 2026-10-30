# Getting started

`localagent` is a local-first coding-agent runtime. Its permission engine, tool
runtime, and persistence are deterministic; the model backend supplies the
probabilistic response.

## Prerequisites

- CMake 3.24 or newer
- Ninja
- A C++23 compiler
- libcurl and SQLite3 development packages

CMake fetches Catch2, nlohmann/json, and spdlog during configuration.

## Build and verify

From the repository root:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

The test suite includes unit tests and CLI integration tests. The integration
tests use the fake backend, so they do not need Ollama or a downloaded model.

## Run without a model service

Use the fake model to validate the CLI, JSON format, session persistence, or a
local integration without network access:

```bash
LOCALAGENT_FAKE_MODEL=1 ./build/localagent \
  -p "summarize the current task" \
  --output-format json
```

The bundled fake backend has scripted responses. It is intended for smoke tests
and CI, not for useful model-generated answers.

## Run with Ollama

By default, localagent connects to Ollama at `http://127.0.0.1:11434`. Start an
Ollama service with a model available locally, then run a prompt:

```bash
./build/localagent chat -p "summarize this repository"
./build/localagent agent -p "fix the failing test"
```

If Ollama is unreachable, the executable falls back to an empty fake backend;
the command remains local but cannot produce a meaningful response. Set
`LOCALAGENT_OLLAMA_HOST` to use a different local endpoint.

## Inspect the CLI

```bash
./build/localagent --help
./build/localagent --version
./build/localagent models list
```

Pass a prompt either positionally or with `-p` / `--prompt`. For automation,
use `--output-format json`.
