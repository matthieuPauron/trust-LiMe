#!/usr/bin/env bash
# OPT-IN: install post-commit/post-merge hooks in the TRUST and TRIOCFD repos
# that rebuild the corresponding codebase index. Run once per machine.
#
# Usage:
#   .opencode/scripts/install-git-hooks.sh [--trust] [--trio] [--remove]
#
# Hooks are appended (not overwritten) and are idempotent. They call
# index-codebases.sh with --force. The hook files live in the source repos'
# .git/hooks and are NOT version-controlled, so re-run after a fresh clone.
set -u

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
INDEX_SCRIPT="$REPO_ROOT/.opencode/scripts/index-codebases.sh"
TRUST_REPO="${TRUST_REPO:-/home/matthieu/Development/trust-code}"
TRIO_REPO="${TRIO_REPO:-/home/matthieu/Development/triocfd-code}"

DO_TRUST=0; DO_TRIO=0; REMOVE=0
for arg in "$@"; do
    case "$arg" in
        --trust) DO_TRUST=1 ;;
        --trio)  DO_TRIO=1 ;;
        --remove) REMOVE=1 ;;
        --all|"") DO_TRUST=1; DO_TRIO=1 ;;
        *) echo "unknown arg: $arg" >&2; exit 2 ;;
    esac
done
[ "$DO_TRUST" -eq 0 ] && [ "$DO_TRIO" -eq 0 ] && DO_TRUST=1 && DO_TRIO=1

install_one() {
    local repo="$1" name="$2" hookdir="$repo/.git/hooks"
    if [ ! -d "$hookdir" ]; then
        echo "[$name] no .git/hooks in $repo (skip)" >&2
        return 1
    fi
    for hook in post-commit post-merge; do
        local target="$hookdir/$hook"
        if [ "$REMOVE" -eq 1 ]; then
            if [ -f "$target" ]; then
                # remove only our appended block
                python3 - "$target" <<'PY'
import sys
p=sys.argv[1]
s=open(p).read()
marker="\n# trust-LiMe codebase index refresh\n"
i=s.find(marker)
if i!=-1:
    s=s[:i]+"\n"
    open(p,"w").write(s)
    print("removed hook block from",p)
PY
            fi
            continue
        fi
        if [ ! -f "$target" ]; then
            printf '#!/bin/sh\n' > "$target"
        fi
        if grep -q "index-codebases.sh" "$target" 2>/dev/null; then
            echo "[$name] $hook already wired"
            continue
        fi
        {
            echo ""
            echo "# trust-LiMe codebase index refresh"
            echo "\"$INDEX_SCRIPT\" $name --force >/dev/null 2>&1 || true"
        } >> "$target"
        chmod +x "$target"
        echo "[$name] installed $hook"
    done
}

[ "$DO_TRUST" -eq 1 ] && install_one "$TRUST_REPO" trust
[ "$DO_TRIO" -eq 1 ] && install_one "$TRIO_REPO" trio
echo "done."