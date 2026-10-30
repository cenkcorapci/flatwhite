# Configuration and sessions

Configuration is assembled in this order:

1. Built-in defaults
2. `~/.config/flatwhite/config.toml`
3. `.agent/config.toml` in the current working directory
4. `FLATWHITE_*` environment variables
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
# Default: all registered development tools execute without approval.
unrestricted = true
network = "allow"
outside_workspace_write = "allow"
credentials = "allow"
destructive = "allow"
privilege = "allow"

[web_search]
# `{query}` is replaced with a percent-encoded search query.
endpoint = "https://html.duckduckgo.com/html/?q={query}"

[execution]
timeout_seconds = 120
max_parallel_tools = 4
```

`unrestricted` defaults to `true`: all risk levels are permitted without an
approval flow, including filesystem writes, shell commands, git operations,
and network-backed web searches. Classification and risk metadata remain
available for observability. This is an explicit trust boundary; use the agent
only in a workspace and environment you trust.

Set `unrestricted = false` to use the risk-specific policies. Permission
values are `allow`, `ask`, `deny`, and `sandbox`; non-interactive `ask` is not
automatically approved.

## Web search

The registered `web_search` tool performs a GET request against
`[web_search].endpoint`. The endpoint must include `{query}` at the location
where the percent-encoded search query belongs. If it does not, Flatwhite
appends `q=<encoded query>`. The default needs no credentials and uses
DuckDuckGo's HTML endpoint. Choose an internal or other provider by changing
the endpoint; do not put credentials in the endpoint or project config.

## Environment and command-line overrides

The configuration loader recognizes environment variables matching supported
settings, including:

```bash
export FLATWHITE_FAKE_MODEL=1
export FLATWHITE_OLLAMA_HOST=http://127.0.0.1:11434
export FLATWHITE_DEFAULT_MODEL=llama3.2
export FLATWHITE_DATA_DIR="$HOME/.flatwhite"
export FLATWHITE_PERMISSIONS_NETWORK=deny
export FLATWHITE_PERMISSIONS_UNRESTRICTED=false
export FLATWHITE_WEB_SEARCH_ENDPOINT='http://127.0.0.1:8080/search?q={query}'
export FLATWHITE_EXECUTION_TIMEOUT_SECONDS=60
```

The executable also supports:

```bash
./build/flatwhite --fake-model --workspace /path/to/project \
  --data-dir ./flatwhite-data -p "review this change"
```

`--fake-model`, `--workspace`, and `--data-dir` override the loaded
configuration for that invocation.

## Sessions and event data

By default, Flatwhite stores its state under `~/.flatwhite`. Change the
location with `FLATWHITE_DATA_DIR` or `--data-dir`, particularly in CI where a
temporary directory keeps runs isolated.

Each prompt creates a session. List stored sessions with:

```bash
./build/flatwhite sessions list
```

Resume the most recent session with `--continue`, or resume a specific session:

```bash
./build/flatwhite chat --continue -p "continue the investigation"
./build/flatwhite sessions resume <session-id> -p "continue the investigation"
```

The data directory contains the SQLite session store and JSONL event log. Treat
it as local operational data and do not commit it.
