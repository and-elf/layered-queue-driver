#!/usr/bin/env bash
set -euo pipefail

# Robust wrapper: generate app.dts/prj.conf from requirements then call `west build`.
# Locates repository root by walking up until it finds CMakeLists.txt, .git, or west.yml.

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)

find_repo_root() {
    dir="$1"
    while [ "$dir" != "/" ]; do
        if [ -f "$dir/CMakeLists.txt" ] || [ -d "$dir/.git" ] || [ -f "$dir/west.yml" ]; then
            echo "$dir"
            return 0
        fi
        dir=$(dirname "$dir")
    done
    return 1
}

ROOT=$(find_repo_root "$SCRIPT_DIR") || ROOT=$(cd "${SCRIPT_DIR}/../../.." && pwd)

PYTHON="${ROOT}/.venv/bin/python3"
if [ ! -x "${PYTHON}" ]; then
    PYTHON=python3
fi

REQGEN="${SCRIPT_DIR}/reqgen.py"
DTSGEN="${SCRIPT_DIR}/dts_gen.py"

BOARD=""
WEST_ARGS=()

while [[ $# -gt 0 ]]; do
    case "$1" in
        -b|--board)
            BOARD="$2"; shift 2;;
        --)
            shift; WEST_ARGS+=("$@"); break;;
        *)
            WEST_ARGS+=("$1"); shift;;
    esac
done

if [ -z "$BOARD" ]; then
    echo "Usage: $0 -b <board> [west args]" >&2
    exit 2
fi

PREREQ_DIR=${ROOT}/build/prereq
mkdir -p "${PREREQ_DIR}"

echo "Generating app.dts into ${PREREQ_DIR} using requirements at ${ROOT}/requirements"
"${PYTHON}" "${REQGEN}" "${ROOT}/requirements" generate-dts -o "${PREREQ_DIR}/app.dts" || {
    echo "reqgen.py failed" >&2; exit 1;
}

echo "Running dts_gen to produce sources and prj.conf"
"${PYTHON}" "${DTSGEN}" "${PREREQ_DIR}/app.dts" "${PREREQ_DIR}" || {
    echo "dts_gen.py failed" >&2; exit 1;
}

echo "Invoking west build for board ${BOARD}"
west build -b "${BOARD}" "${WEST_ARGS[@]}"
#!/usr/bin/env bash
set -euo pipefail

# Wrapper: generate app.dts/prj.conf from requirements then call `west build`.

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "${SCRIPT_DIR}/.." && pwd)

PYTHON=${ROOT}/.venv/bin/python3
if [ ! -x "${PYTHON}" ]; then
    PYTHON=python3
fi

REQGEN="${ROOT}/layered-queue-driver/scripts/reqgen.py"
DTSGEN="${ROOT}/layered-queue-driver/scripts/dts_gen.py"

BOARD=""
WEST_ARGS=()

while [[ $# -gt 0 ]]; do
    case "$1" in
        -b|--board)
            BOARD="$2"; shift 2;;
        --)
            shift; WEST_ARGS+=("$@"); break;;
        *)
            WEST_ARGS+=("$1"); shift;;
    esac
done

if [ -z "$BOARD" ]; then
    echo "Usage: $0 -b <board> [west args]" >&2
    exit 2
fi

PREREQ_DIR=${ROOT}/build/prereq
mkdir -p "${PREREQ_DIR}"

echo "Generating app.dts into ${PREREQ_DIR} using requirements at ${ROOT}/requirements"
"${PYTHON}" "${REQGEN}" "${ROOT}/requirements" generate-dts -o "${PREREQ_DIR}/app.dts" || {
    echo "reqgen.py failed" >&2; exit 1;
}

echo "Running dts_gen to produce sources and prj.conf"
"${PYTHON}" "${DTSGEN}" "${PREREQ_DIR}/app.dts" "${PREREQ_DIR}" || {
    echo "dts_gen.py failed" >&2; exit 1;
}

echo "Invoking west build for board ${BOARD}"
west build -b "${BOARD}" "${WEST_ARGS[@]}"
