# Getting started

`flatwhite` is a local-first coding-agent runtime. Its permission engine, tool
runtime, and persistence are deterministic; the model backend supplies the
probabilistic response.

## Install

For Ubuntu/Debian and macOS with Homebrew, the installer provisions the build
dependencies and installs Flatwhite:

```bash
curl -fsSL https://raw.githubusercontent.com/cenkcorapci/flatwhite/main/install.sh | sh
```

It uses `sudo` only when Ubuntu/Debian package installation requires it. On
other systems, install CMake 3.24 or newer, Ninja or make, a C++23 compiler,
Git, curl, and libcurl/SQLite development packages, then rerun the command.
CMake obtains any missing C++ dependencies during configuration.
When Fish is installed, the installer adds the install directory's `bin`
subdirectory to Fish's universal `PATH`.

## Build and verify

From the repository root:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

The test suite includes unit tests and CLI integration tests. The integration
tests use the fake backend, so they do not need Ollama or a downloaded model.

Run each level explicitly with:

```bash
./build/flatwhite_tests
ctest --test-dir build -R '^integration_' --output-on-failure
ctest --test-dir build --output-on-failure
```

Development tools (filesystem search/edit, shell, git, and web search) are
unrestricted by default. Treat the workspace and configured web-search
provider as a trust boundary. See [configuration](configuration.md) to opt
into restricted policies or configure web search.

## Run without a model service

Use the fake model to validate the CLI, JSON format, session persistence, or a
local integration without network access:

```bash
FLATWHITE_FAKE_MODEL=1 ./build/flatwhite \
  -p "summarize the current task" \
  --output-format json
```

The bundled fake backend has scripted responses. It is intended for smoke tests
and CI, not for useful model-generated answers.

## Run with Ollama

By default, Flatwhite connects to Ollama at `http://127.0.0.1:11434`. Start an
Ollama service with a model available locally, then run a prompt:

```bash
./build/flatwhite
./build/flatwhite chat -p "summarize this repository"
./build/flatwhite agent -p "fix the failing test"
```

If Ollama is unreachable, the executable falls back to an empty fake backend;
the command remains local but cannot produce a meaningful response. Set
`FLATWHITE_OLLAMA_HOST` to use a different local endpoint. In interactive mode,
`/model` lists models installed in local Ollama and selects an exact or uniquely
matching prefix. It only chooses the model for future requests; it does not
bypass provider or safety behavior.

## Inspect the CLI

```bash
./build/flatwhite --help
./build/flatwhite --version
./build/flatwhite models list
```

Pass a prompt either positionally or with `-p` / `--prompt`. For automation,
use `--output-format json`.
