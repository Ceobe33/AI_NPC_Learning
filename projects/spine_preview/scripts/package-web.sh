#!/usr/bin/env bash
#
# Packs the compiled WebAssembly studio into a portable page bundle that drops
# into any Hexo site: no theme layout, no front matter, no build step on the
# blog side.
#
# The bundle is the contents of web/source plus the two compiled files:
#
#   source/spine-studio/index.html   stand-alone page  -> /spine-studio/
#   source/spine-studio/*.js, *.wasm the module itself
#   source/studio/index.md           page + iframe     -> /studio/
#
# Usage:
#   ./scripts/package-web.sh                        # -> release/spine-studio-web
#   ./scripts/package-web.sh -o /tmp/studio
#   ./scripts/package-web.sh --install ~/blog/source
#   WASM_DIR=/path/to/wasm ./scripts/package-web.sh
#
# Nothing is deleted: the output directory is written over, so re-running after
# a rebuild just refreshes the module.
#
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

TEMPLATE="${PROJECT_DIR}/web/source"
OUT="${PROJECT_DIR}/release/spine-studio-web"
INSTALL=""
# Where build-wasm.sh left its output. Override when that build went elsewhere.
WASM_DIR="${WASM_DIR:-${PROJECT_DIR}/../../site/source/spine-studio}"

usage() {
    sed -n '3,20p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
    exit 0
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        -o|--out)      OUT="$2"; shift 2 ;;
        --wasm-dir)    WASM_DIR="$2"; shift 2 ;;
        --install)     INSTALL="$2"; shift 2 ;;
        -h|--help)     usage ;;
        *) echo "error: unknown argument: $1" >&2; usage ;;
    esac
done

JS="spine-animation-studio.js"
WASM="spine-animation-studio.wasm"

if [[ ! -d "${TEMPLATE}" ]]; then
    echo "error: no page template at ${TEMPLATE}" >&2
    exit 1
fi

for file in "${JS}" "${WASM}"; do
    if [[ ! -f "${WASM_DIR}/${file}" ]]; then
        echo "error: ${WASM_DIR}/${file} is missing" >&2
        echo >&2
        echo "Build the module first:" >&2
        echo "  EMSDK=~/emsdk ./scripts/build-wasm.sh" >&2
        echo >&2
        echo "Or point this script at an existing build:" >&2
        echo "  ./scripts/package-web.sh --wasm-dir /path/to/build" >&2
        exit 1
    fi
done

mkdir -p "${OUT}/source"
cp -R "${TEMPLATE}/." "${OUT}/source/"
# Instructions travel with the bundle; they sit outside source/ so they are not
# copied into the blog itself.
cp "${PROJECT_DIR}/web/README.md" "${OUT}/README.md"
mkdir -p "${OUT}/source/spine-studio"
cp "${WASM_DIR}/${JS}" "${WASM_DIR}/${WASM}" "${OUT}/source/spine-studio/"

TARGET="${INSTALL:-${OUT}/source}"

if [[ -n "${INSTALL}" ]]; then
    if [[ ! -d "${INSTALL}" ]]; then
        echo "error: ${INSTALL} is not a directory" >&2
        echo "Pass the source directory of the target Hexo site, e.g. ~/blog/source" >&2
        exit 1
    fi

    # _posts/ turns these into dated articles without a stable URL, and any other
    # underscore-prefixed directory is skipped by Hexo outright.
    if [[ "${INSTALL}" == */_post || "${INSTALL}" == */_post/* \
       || "${INSTALL}" == */_posts || "${INSTALL}" == */_posts/* ]]; then
        echo "error: ${INSTALL} is inside a _posts directory" >&2
        echo >&2
        echo "These are pages, not posts: copy them into the site's plain source/" >&2
        echo "directory, e.g. ~/blog/source" >&2
        exit 1
    fi

    cp -R "${OUT}/source/." "${INSTALL}/"

    for file in "spine-studio/index.html" "spine-studio/${JS}" \
                "spine-studio/${WASM}" "studio/index.md"; do
        if [[ ! -f "${INSTALL}/${file}" ]]; then
            echo "error: ${INSTALL}/${file} was not written" >&2
            exit 1
        fi
    done
fi

size() { ls -lh "$1" | awk '{print $5}'; }

cat <<EOF

packaged:
  ${OUT}/source/spine-studio/index.html
  ${OUT}/source/spine-studio/${JS}   ($(size "${OUT}/source/spine-studio/${JS}"))
  ${OUT}/source/spine-studio/${WASM} ($(size "${OUT}/source/spine-studio/${WASM}"))
  ${OUT}/source/studio/index.md

installed into: ${TARGET}

next:
  1. 把 ${OUT}/source 里的 spine-studio/ 和 studio/ 拷进目标 Hexo 的 source/
     （这次已经拷好了，除非上面 --install 指向别处）
  2. hexo clean && hexo generate
  3. 打开 /spine-studio/（整页）或 /studio/（套在博客主题里）

整页那个 index.html 自带 layout: false，不需要动博客的 _config.yml。
想挂到主题导航上：主题 _config.yml 的 menu / nav 加一项指向 /spine-studio/。
细节和坑都写在 ${OUT}/README.md。
EOF
