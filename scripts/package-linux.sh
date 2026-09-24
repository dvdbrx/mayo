#!/usr/bin/env bash
set -euo pipefail

# CadView Linux Packaging Script
# Creates a standalone, relocatable portable distribution archive.
# Enforces the 70 MB package budget.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

cd "${REPO_ROOT}"

echo "=== Packaging CadView for Linux (x86_64) ==="

BUILD_DIR="${REPO_ROOT}/build"
CADVIEW_BIN="${BUILD_DIR}/cadview"

if [ ! -x "${CADVIEW_BIN}" ]; then
    echo "Building CadView release binary..."
    "${REPO_ROOT}/scripts/with-env.sh" cmake --build "${BUILD_DIR}" --target mayo -j$(nproc)
fi

DIST_NAME="cadview-linux-x86_64"
DIST_DIR="${REPO_ROOT}/dist/${DIST_NAME}"
ARCHIVE_PATH="${REPO_ROOT}/dist/${DIST_NAME}.tar.gz"

rm -rf "${DIST_DIR}" "${ARCHIVE_PATH}"
mkdir -p "${DIST_DIR}/bin" "${DIST_DIR}/lib" "${DIST_DIR}/resources" "${REPO_ROOT}/dist"

# Copy binary and strip
cp "${CADVIEW_BIN}" "${DIST_DIR}/bin/cadview"
strip -s "${DIST_DIR}/bin/cadview"

# Copy Legal Notices
cp "${REPO_ROOT}/LICENSE" "${DIST_DIR}/"
cp "${REPO_ROOT}/THIRD_PARTY.md" "${DIST_DIR}/"

# Copy Qt plugins
SYSROOT="${REPO_ROOT}/.deps/sysroot/usr/lib/x86_64-linux-gnu"
QT_PLUGINS="${SYSROOT}/qt6/plugins"
if [ -d "${QT_PLUGINS}" ]; then
    mkdir -p "${DIST_DIR}/plugins"
    cp -r "${QT_PLUGINS}/platforms" "${DIST_DIR}/plugins/"
fi

# Copy Shaders
SHADERS="${REPO_ROOT}/.deps/sysroot/usr/share/opencascade/resources/Shaders"
if [ -d "${SHADERS}" ]; then
    cp -r "${SHADERS}" "${DIST_DIR}/resources/Shaders"
fi

# Resolve and bundle sysroot dependencies recursively
if [ -d "${SYSROOT}" ]; then
    echo "Collecting shared runtime dependencies recursively..."
    export LD_LIBRARY_PATH="${SYSROOT}:${REPO_ROOT}/.deps/sysroot/usr/lib:${LD_LIBRARY_PATH:-}"

    declare -A SEEN
    TODO=("${DIST_DIR}/bin/cadview")
    if [ -d "${DIST_DIR}/plugins" ]; then
        while IFS= read -r -d '' p; do
            TODO+=("${p}")
        done < <(find "${DIST_DIR}/plugins" -type f -name "*.so" -print0)
    fi

    while [ ${#TODO[@]} -gt 0 ]; do
        CURRENT="${TODO[0]}"
        TODO=("${TODO[@]:1}")

        while IFS= read -r line; do
            if [[ "${line}" =~ "=>" ]]; then
                libpath=$(echo "${line}" | awk '{print $3}')
                if [[ -n "${libpath}" && "${libpath}" == "${SYSROOT}"* && -f "${libpath}" ]]; then
                    real_file=$(realpath "${libpath}")
                    base_real=$(basename "${real_file}")
                    soname=$(basename "${libpath}")
                    if [ -z "${SEEN[${base_real}]:-}" ]; then
                        SEEN[${base_real}]=1
                        cp "${real_file}" "${DIST_DIR}/lib/"
                        if [ "${soname}" != "${base_real}" ]; then
                            (cd "${DIST_DIR}/lib" && ln -sf "${base_real}" "${soname}")
                        fi
                        TODO+=("${real_file}")
                    fi
                fi
            fi
        done < <(ldd "${CURRENT}" 2>/dev/null || true)
    done

    # Strip libraries and plugins
    find "${DIST_DIR}/lib" "${DIST_DIR}/plugins" -type f \( -name "*.so" -o -name "*.so.*" \) -exec strip -s {} + 2>/dev/null || true
fi


# Create launcher script
cat <<'EOF' > "${DIST_DIR}/cadview.sh"
#!/usr/bin/env bash
set -e
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export LD_LIBRARY_PATH="${HERE}/lib:${LD_LIBRARY_PATH:-}"
export QT_PLUGIN_PATH="${HERE}/plugins:${QT_PLUGIN_PATH:-}"
if [ -d "${HERE}/resources/Shaders" ]; then
    export CSF_ShadersDirectory="${HERE}/resources/Shaders"
fi
exec "${HERE}/bin/cadview" "$@"
EOF
chmod +x "${DIST_DIR}/cadview.sh"

# Create archive
echo "Creating compressed archive..."
tar -czf "${ARCHIVE_PATH}" -C "${REPO_ROOT}/dist" "${DIST_NAME}"

# Budget check: ≤ 70 MB
MAX_BYTES=73400320
ARCHIVE_SIZE=$(stat -c%s "${ARCHIVE_PATH}")
ARCHIVE_MB=$(awk "BEGIN {printf \"%.2f\", ${ARCHIVE_SIZE}/1048576}")

echo "Package created: ${ARCHIVE_PATH} (${ARCHIVE_MB} MB)"

if [ "${ARCHIVE_SIZE}" -gt "${MAX_BYTES}" ]; then
    echo "ERROR: Package size ${ARCHIVE_MB} MB exceeds budget limit of 70 MB!" >&2
    exit 1
fi

echo "✓ Package budget assertion passed (${ARCHIVE_MB} MB ≤ 70.00 MB)"
echo "=== Packaging completed successfully ==="
