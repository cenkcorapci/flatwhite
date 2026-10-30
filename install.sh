#!/bin/sh
# Install localagent from its public source repository without downloading a model.
set -eu

REPO_URL=${LOCALAGENT_REPO_URL:-https://github.com/cenkcorapci/flatwhite.git}
REPO_REF=${LOCALAGENT_REPO_REF:-main}
INSTALL_DIR=${LOCALAGENT_INSTALL_DIR:-"$HOME/.localagent"}
SOURCE_DIR="$INSTALL_DIR/src/flatwhite"
BUILD_DIR="$SOURCE_DIR/build"
BIN="$INSTALL_DIR/bin/localagent"

install_dependencies() {
  if command -v apt-get >/dev/null 2>&1; then
    if [ "$(id -u)" -eq 0 ]; then
      apt-get update
      apt-get install -y --no-install-recommends \
        build-essential cmake ninja-build git curl ca-certificates \
        libcurl4-openssl-dev libsqlite3-dev libspdlog-dev nlohmann-json3-dev
      return
    fi
    if command -v sudo >/dev/null 2>&1; then
      sudo apt-get update
      sudo apt-get install -y --no-install-recommends \
        build-essential cmake ninja-build git curl ca-certificates \
        libcurl4-openssl-dev libsqlite3-dev libspdlog-dev nlohmann-json3-dev
      return
    fi
    echo "localagent installer: apt-get needs root or sudo to install dependencies." >&2
    exit 1
  fi

  if command -v brew >/dev/null 2>&1; then
    brew install cmake ninja curl sqlite spdlog nlohmann-json
    return
  fi

  echo "localagent installer: install Git, CMake, a C++23 compiler, curl, SQLite, libcurl, and Ninja or make, then rerun." >&2
  exit 1
}

install_dependencies

for command in git cmake c++ curl; do
  if ! command -v "$command" >/dev/null 2>&1; then
    echo "localagent installer: required command not found: $command" >&2
    exit 1
  fi
done

if command -v ninja >/dev/null 2>&1; then
  generator=Ninja
else
  if ! command -v make >/dev/null 2>&1; then
    echo "localagent installer: install Ninja or make, then rerun." >&2
    exit 1
  fi
  generator="Unix Makefiles"
fi

mkdir -p "$INSTALL_DIR/src"
if [ -e "$SOURCE_DIR" ]; then
  if [ ! -d "$SOURCE_DIR/.git" ]; then
    echo "localagent installer: $SOURCE_DIR exists but is not a git checkout; refusing to replace it." >&2
    exit 1
  fi
  echo "localagent installer: reusing existing checkout at $SOURCE_DIR" >&2
else
  git clone --depth 1 --branch "$REPO_REF" "$REPO_URL" "$SOURCE_DIR"
fi

cmake -S "$SOURCE_DIR" -B "$BUILD_DIR" -G "$generator" \
  -DCMAKE_BUILD_TYPE=Release -DLOCALAGENT_BUILD_TESTS=OFF
cmake --build "$BUILD_DIR" --parallel
cmake --install "$BUILD_DIR" --prefix "$INSTALL_DIR"

if [ ! -x "$BIN" ]; then
  echo "localagent installer: install completed without $BIN" >&2
  exit 1
fi

(
  cd "$SOURCE_DIR"
  LOCALAGENT_FAKE_MODEL=1 LOCALAGENT_DATA_DIR="$INSTALL_DIR/installer-state" \
    "$BIN" -p "installer verification" --output-format json >/dev/null
)
echo "localagent installed at $BIN (verified with the offline fake backend)." >&2
echo "Run: $BIN --help" >&2

if [ "${LOCALAGENT_RUN:-0}" = "1" ]; then
  exec "$BIN" --help
fi
