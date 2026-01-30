#!/usr/bin/env bash
set -euo pipefail

# build_firmware.sh
# Usage: build_firmware.sh [-b BOARD] [--pristine] [--install-sdk] [--hil] [-r REQ_DIR]...
#
# This script bootstraps the workspace (creates .venv, west, etc.), optionally
# installs the Zephyr SDK, and builds firmware using the provided requirement
# directories. If one or more `-r/--requirements` are provided, they will be
# merged into a temporary `requirements/` for the duration of the build.

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
# Repository root: go up three levels from modules/layered-queue-driver/scripts -> workspace root
ROOT=$(cd "${SCRIPT_DIR}/../../.." && pwd)
BOOTSTRAP="${SCRIPT_DIR}/bootstrap_zephyr_project.py"
# Use the wrapper colocated with this script for robust paths
WEST_WRAPPER="${SCRIPT_DIR}/west_build_with_prjconf.sh"

BOARD=""
PRISTINE=""
INSTALL_SDK=""
HIL_TESTS=""
REQS=()
EXTRA_WEST_ARGS=()
ZEPHYR_BASE_OVERRIDE=""

print_usage(){
    cat <<EOF
Usage: $0 [options]

Options:
    -b BOARD            Board name (default: autodiscover from LLRs)
  -r, --requirements  Path to requirements directory (can be specified multiple times)
  --pristine          Pass --pristine to west build
  --install-sdk       Download and attempt to install Zephyr SDK
  --hil               Enable HIL tests (passes -DENABLE_HIL_TESTS=ON to CMake)
  --help              Show this help
  -- [extra west args] Any additional args after -- are passed to west build

Environment variables:
    ZEPHYR_BASE         If set, used as Zephyr source path. If unset and
                                            ~/.config/zephyr exists, that path will be used.

This will bootstrap the project, merge provided requirements (if any), then
generate app.dts/prj.conf and run `west build`.
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        -b|--board)
            BOARD="$2"; shift 2;;
        -z|--zephyr-base)
            ZEPHYR_BASE_OVERRIDE="$2"; shift 2;;
        --pristine)
            PRISTINE="--pristine"; shift;;
        --install-sdk)
            INSTALL_SDK="--install-sdk"; shift;;
        --hil)
            HIL_TESTS=1; shift;;
        -r|--requirements)
            REQS+=("$2"); shift 2;;
        -z|--zephyr-base)
            ZEPHYR_BASE_OVERRIDE="$2"; shift 2;;
        --help)
            print_usage; exit 0;;
        --)
            shift; EXTRA_WEST_ARGS+=("$@"); break;;
        *)
            # Collect unknown args to pass to west
            EXTRA_WEST_ARGS+=("$1"); shift;;
    esac
done

echo "Build firmware: board=${BOARD} reqs=${REQS[*]} hil=${HIL_TESTS} install-sdk=${INSTALL_SDK}"

# Step 1: Run bootstrap (creates .venv and west)
# Determine ZEPHYR_BASE to use: explicit override -> env var -> ~/.config/zephyr if present
if [ -n "${ZEPHYR_BASE_OVERRIDE}" ]; then
    echo "Using provided ZEPHYR_BASE: ${ZEPHYR_BASE_OVERRIDE}"
    export ZEPHYR_BASE="${ZEPHYR_BASE_OVERRIDE}"
elif [ -n "${ZEPHYR_BASE:-}" ]; then
    echo "Using ZEPHYR_BASE from environment: ${ZEPHYR_BASE}"
else
    DEFAULT_ZEPHYR="$HOME/.config/zephyr"
    if [ -d "$DEFAULT_ZEPHYR" ]; then
        echo "Found Zephyr at ${DEFAULT_ZEPHYR}; using as ZEPHYR_BASE"
        export ZEPHYR_BASE="$DEFAULT_ZEPHYR"
    fi
fi

echo "Running bootstrap..."
# If ZEPHYR_BASE is already set in environment, bootstrap will detect Zephyr sources
python3 "${BOOTSTRAP}" ${INSTALL_SDK:+--install-sdk}

# Activate virtualenv for subsequent commands
source "${ROOT}/.venv/bin/activate"

# Step 2: If requirements provided, create a merged temporary requirements dir
TMP_REQ_DIR="${ROOT}/build/requirements_tmp_$$"
BACKUP_REQ_DIR=""
if [ ${#REQS[@]} -gt 0 ]; then
    echo "Merging ${#REQS[@]} requirements directories into ${TMP_REQ_DIR}"
    rm -rf "${TMP_REQ_DIR}"
    mkdir -p "${TMP_REQ_DIR}/high-level" "${TMP_REQ_DIR}/low-level"
    for r in "${REQS[@]}"; do
        if [ -d "$r/high-level" ]; then
            cp -r "$r/high-level/"* "${TMP_REQ_DIR}/high-level/" 2>/dev/null || true
        fi
        if [ -d "$r/low-level" ]; then
            cp -r "$r/low-level/"* "${TMP_REQ_DIR}/low-level/" 2>/dev/null || true
        fi
    done

    # Backup existing requirements if present
    if [ -d "${ROOT}/requirements" ]; then
        BACKUP_REQ_DIR="${ROOT}/requirements_backup_$$"
        echo "Backing up existing requirements to ${BACKUP_REQ_DIR}"
        mv "${ROOT}/requirements" "${BACKUP_REQ_DIR}"
    fi

    # Move temp into place
    mv "${TMP_REQ_DIR}" "${ROOT}/requirements"
    trap 'echo Restoring original requirements; if [ -n "${BACKUP_REQ_DIR}" ] && [ -d "${BACKUP_REQ_DIR}" ]; then rm -rf "${ROOT}/requirements"; mv "${BACKUP_REQ_DIR}" "${ROOT}/requirements"; fi' EXIT
fi

# Auto-detect board from requirements if not provided
if [ -z "${BOARD}" ]; then
    REQ_DIR="${ROOT}/requirements"
    if [ ! -d "${REQ_DIR}" ]; then
        echo "No requirements directory found at ${REQ_DIR}; cannot determine board" >&2
        exit 2
    fi
    echo "Detecting board from requirements..."
    META_JSON_RAW=$(python3 "${SCRIPT_DIR}/reqgen.py" "${REQ_DIR}" metadata 2>/dev/null || true)
    # Extract last non-empty line (reqgen may print info lines) to get JSON
    META_JSON=$(printf '%s' "$META_JSON_RAW" | awk 'NF{line=$0} END{print line}')
    if [ -z "${META_JSON}" ]; then
        echo "Failed to detect board from requirements" >&2
        exit 2
    fi
    BOARD=$(printf '%s' "$META_JSON" | python3 -c "import sys,json;print(json.load(sys.stdin).get('board') or '')")
    OVERLAY=$(printf '%s' "$META_JSON" | python3 -c "import sys,json;print(json.load(sys.stdin).get('overlay') or '')")
    if [ -z "${BOARD}" ]; then
        echo "No board declared in LLRs; please specify -b/--board" >&2
        exit 2
    fi
    echo "Detected board: ${BOARD}"
    if [ -n "${OVERLAY}" ]; then
        echo "Detected overlay: ${OVERLAY}"
        # Copy overlay into project's boards directory so Zephyr can find it
        mkdir -p "${ROOT}/boards"
        # Resolve overlay path relative to requirements if not absolute
        if [[ "${OVERLAY}" != /* ]]; then
            OVERLAY_SRC="${REQ_DIR}/${OVERLAY}"
        else
            OVERLAY_SRC="${OVERLAY}"
        fi
        if [ -f "${OVERLAY_SRC}" ]; then
            cp "${OVERLAY_SRC}" "${ROOT}/boards/${BOARD}.overlay"
            echo "Copied overlay to ${ROOT}/boards/${BOARD}.overlay"
        else
            echo "Warning: overlay file not found: ${OVERLAY_SRC}" >&2
        fi
    fi
fi

# Step 3: Build via wrapper that generates app.dts/prj.conf then calls west
BUILD_CMD=("${WEST_WRAPPER}" -b "${BOARD}" ${PRISTINE})

if [ -n "${HIL_TESTS}" ]; then
    # Pass CMake variable through west (after --)
    EXTRA_WEST_ARGS+=("--" "-DENABLE_HIL_TESTS=ON")
fi

echo "Running build: ${BUILD_CMD[*]} ${EXTRA_WEST_ARGS[*]}"
"${WEST_WRAPPER}" -b "${BOARD}" ${PRISTINE} "${EXTRA_WEST_ARGS[@]}"

echo "Build finished."
