#!/usr/bin/env bash
# Look up a symbol in the TRUST/TRIOCFD codebase indexes.
#
# Usage:
#   find-symbol.sh <name> [--trust|--trio|--both] [--kind KIND] [--context N] [--count]
#
# Output:  file:line  kind  prototype/snippet
# Requires indexes built by index-codebases.sh (in .index/).
set -u

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
INDEX_DIR="$REPO_ROOT/.index"

NAME=""
SCOPE="both"
KIND=""
CTX=0
COUNT=0

while [ $# -gt 0 ]; do
    case "$1" in
        --trust) SCOPE="trust" ;;
        --trio)  SCOPE="trio" ;;
        --both)  SCOPE="both" ;;
        --kind)  KIND="$2"; shift ;;
        --context) CTX="$2"; shift ;;
        --count) COUNT=1 ;;
        -h|--help)
            sed -n '2,12p' "$0"; exit 0 ;;
        *)
            if [ -z "$NAME" ]; then NAME="$1"; else echo "unexpected arg: $1" >&2; exit 2; fi
            ;;
    esac
    shift
done

if [ -z "$NAME" ]; then
    echo "usage: find-symbol.sh <name> [--trust|--trio] [--kind KIND] [--context N] [--count]" >&2
    exit 2
fi

# --- staleness warning (default on) ---
if [ "${TRUST_INDEX_NO_CHECK:-0}" != "1" ]; then
    "$(dirname "${BASH_SOURCE[0]}")/index-codebases.sh" "$SCOPE" --check 2>&1 1>/dev/null | grep -iE 'STALE|NO INDEX' >&2 || true
fi

files=()
case "$SCOPE" in
    trust) files=("$INDEX_DIR/trust.tags") ;;
    trio)  files=("$INDEX_DIR/trio.tags") ;;
    both)  files=("$INDEX_DIR/trust.tags" "$INDEX_DIR/trio.tags") ;;
esac

# Build grep pattern: symbol at start of line (ctags -x format: "NAME  kind  line  file ...")
pattern="^${NAME}[[:space:]]"

total=0
for f in "${files[@]}"; do
    [ -f "$f" ] || { echo "index missing: $f  (run index-codebases.sh)" >&2; continue; }
    # filter by kind if requested (2nd column)
    if [ -n "$KIND" ]; then
        matches=$(grep -E "$pattern" "$f" | awk -v k="$KIND" '$4==k')
    else
        matches=$(grep -E "$pattern" "$f")
    fi
    [ -z "$matches" ] && continue
    n=$(printf '%s\n' "$matches" | wc -l)
    total=$((total + n))
    if [ "$COUNT" -eq 1 ]; then
        echo "$(basename "$f"): $n"
        continue
    fi
    printf '%s\n' "$matches" | while IFS= read -r line; do
        # ctags -x format: symbol kind line file [prototype...]
        sym=$(printf '%s' "$line" | awk '{print $1}')
        lineno=$(printf '%s' "$line" | awk '{print $2}')
        file=$(printf '%s' "$line" | awk '{print $3}')
        kind=$(printf '%s' "$line" | awk '{print $4}')
        proto=$(printf '%s' "$line" | awk '{for(i=5;i<=NF;i++) printf "%s%s", $i, (i<NF?" ":""); print ""}')
        echo "$file:$lineno  [$kind]  $proto"
        if [ "$CTX" -gt 0 ] && [ -f "$file" ]; then
            sed -n "$((lineno-CTX)),$((lineno+CTX))p" "$file" | sed 's/^/    /'
        fi
    done
done

if [ "$COUNT" -eq 0 ]; then
    echo "--- $total match(es) ---" >&2
fi