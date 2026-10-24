#!/usr/bin/env bash
# Build (or rebuild) flat symbol indexes for the TRUST and TRIOCFD codebases.
#
# Usage:
#   .opencode/scripts/index-codebases.sh [trust|trio|both] [--force]
#
# Indexes only the src/ tree of each codebase (headers + .cpp/.hpp) using GNU
# ctags (-x cross-reference output), sorted by symbol name. Output:
#   .index/trust.tags   (from ../trust-code/src)
#   .index/trio.tags    (from ~/Development/triocfd-code/src)
#
# The .index/ directory is gitignored. Re-run with --force after updating a
# codebase; otherwise an existing index is left untouched.
set -u

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
INDEX_DIR="$REPO_ROOT/.index"
TRUST_SRC="${TRUST_SRC:-/home/matthieu/Development/trust-code/src}"
TRIO_SRC="${TRIO_SRC:-/home/matthieu/Development/triocfd-code/src}"

FORCE=0
TARGETS=()
for arg in "$@"; do
    case "$arg" in
        --force|-f) FORCE=1 ;;
        trust|trio|both) TARGETS+=("$arg") ;;
        *) echo "unknown arg: $arg" >&2; exit 2 ;;
    esac
done
[ ${#TARGETS[@]} -eq 0 ] && TARGETS=(both)

mkdir -p "$INDEX_DIR"

build_one() {
    local name="$1"
    local src="$2"
    local out="$INDEX_DIR/$name.tags"
    if [ ! -d "$src" ]; then
        echo "[$name] src not found: $src  (skip)" >&2
        return 1
    fi
    if [ -f "$out" ] && [ "$FORCE" -eq 0 ]; then
        echo "[$name] index exists ($out); use --force to rebuild."
        return 0
    fi
    echo "[$name] indexing $src ..."
    local tmp="$out.tmp"
    find "$src" -type f \( -name '*.h' -o -name '*.cpp' -o -name '*.hpp' -o -name '*.cc' \) -print0 \
        | xargs -0 ctags -x 2>/dev/null \
        | sort -k1,1 > "$tmp"
    if [ ! -s "$tmp" ]; then
        echo "[$name] produced empty index; aborting." >&2
        rm -f "$tmp"
        return 1
    fi
    mv "$tmp" "$out"
    local nfiles nsyms
    nfiles=$(find "$src" -type f \( -name '*.h' -o -name '*.cpp' -o -name '*.hpp' -o -name '*.cc' \) | wc -l)
    nsyms=$(wc -l < "$out")
    echo "[$name] done: $nsyms symbols from $nfiles files -> $out"
    # sidecar metadata
    {
        echo "built: $(date -Is)"
        echo "src: $src"
        echo "files: $nfiles"
        echo "symbols: $nsyms"
        if [ -d "$src/../.git" ]; then
            (cd "$src/.." && echo "git: $(git rev-parse --short HEAD 2>/dev/null)")
        fi
    } > "$out.meta"
}

for t in "${TARGETS[@]}"; do
    case "$t" in
        trust) build_one trust "$TRUST_SRC" ;;
        trio)  build_one trio  "$TRIO_SRC" ;;
        both)  build_one trust "$TRUST_SRC"; build_one trio "$TRIO_SRC" ;;
    esac
done

echo "Index dir: $INDEX_DIR"