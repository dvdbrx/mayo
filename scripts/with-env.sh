#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd -- "$SCRIPT_DIR/.." && pwd)"
SYSROOT="$REPO_DIR/.deps/sysroot/usr"

if [[ ! -d "$SYSROOT" ]]; then
    echo "Dependencies not found in $SYSROOT. Running bootstrap-deps.py..."
    python3 "$REPO_DIR/scripts/bootstrap-deps.py"
fi

export CMAKE_PREFIX_PATH="$SYSROOT${CMAKE_PREFIX_PATH:+:$CMAKE_PREFIX_PATH}"
export LD_LIBRARY_PATH="$SYSROOT/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export QT_PLUGIN_PATH="$SYSROOT/lib/x86_64-linux-gnu/qt6/plugins"
export CSF_ShadersDirectory="$SYSROOT/share/opencascade/resources/Shaders"
export PATH="$SYSROOT/bin:$PATH"

if [[ "${BASH_SOURCE[0]}" != "${0}" ]]; then
    # Script is being sourced: environment variables are now active in current shell
    return 0
fi

if [[ $# -eq 0 ]]; then
    echo "CadView Environment Wrapper"
    echo "Usage:"
    echo "  1. Run command directly:     ./scripts/with-env.sh <command> [args...]"
    echo "     Example:                  ./scripts/with-env.sh ./build/cadview tests/fixtures/F1.step"
    echo "  2. Source into current shell: source ./scripts/with-env.sh"
    echo "     Then run:                 ./build/cadview tests/fixtures/F1.step"
    echo ""
    echo "Spawning an interactive subshell with CadView environment loaded (type 'exit' to leave)..."
    exec "${SHELL:-bash}"
fi

exec "$@"

