# Configuration and sessions

Configuration is assembled in this order:

1. Built-in defaults
2. `~/.config/localagent/config.toml`
3. `.agent/config.toml` in the current working directory
4. `LOCALAGENT_*` environment variables
5. Supported command-line flags

The project configuration is useful for repository-wide defaults. Environment
variables and flags are useful for a one-off invocation or CI.

## Project configuration

Create `.agent/config.toml`:

```toml
[agent]
default_mode = "agent"
max_turns = 40

[context]
default_tokens = 32768
max_tokens = 65536

[models]
backend = "ollama"
auto_route = true
memory_budget_gb = 20

[permissions]
network = "ask"
outside_workspace_write = "deny"
credentials = "deny"
destructive = "ask"
privilege = "deny"

[execution]
timeout_seconds = 120
max_parallel_tools = 4
```

Permission values are `allow`, `ask`, `deny`, and `sandbox`. Keep credentials
and writes outside the workspace denied unless there is a specific, reviewed
need to relax them.

## Environment and command-line overrides

The configuration loader recognizes environment variables matching supported
settings, including:

```bash
export LOCALAGENT_FAKE_MODEL=1
export LOCALAGENT_OLLAMA_HOST=http://127.0.0.1:11434
export LOCALAGENT_DEFAULT_MODEL=llama3.2
export LOCALAGENT_DATA_DIR="$HOME/.localagent"
export LOCALAGENT_PERMISSIONS_NETWORK=deny
export LOCALAGENT_EXECUTION_TIMEOUT_SECONDS=60
```

The executable also supports:

```bash
./build/localagent --fake-model --workspace /path/to/project \
  --data-dir /tmp/localagent-data -p "review this change"
```

`--fake-model`, `--workspace`, and `--data-dir` override the loaded
configuration for that invocation.

## Sessions and event data

By default, localagent stores its state under `~/.localagent`. Change the
location with `LOCALAGENT_DATA_DIR` or `--data-dir`, particularly in CI where a
temporary directory keeps runs isolated.

Each prompt creates a session. List stored sessions with:

```bash
./build/localagent sessions list
```

Resume the most recent session with `--continue`, or resume a specific session:

```bash
./build/localagent chat --continue -p "continue the investigation"
./build/localagent sessions resume <session-id> -p "continue the investigation"
```

The data directory contains the SQLite session store and JSONL event log. Treat
it as local operational data and do not commit it.
