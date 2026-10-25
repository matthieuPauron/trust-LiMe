---
name: trust-code navigation
description: >
  Navigate the TRUST and TRIOCFD source trees (../trust-code, ~/Development/triocfd-code)
  via prebuilt symbol indexes: build indexes with index-codebases.sh, look up
  definitions with find-symbol.sh, and fall back to ripgrep. Use when you need to
  find where a class/function/macro is defined or declared in the kernel or
  TRIOCFD, or to (re)build the indexes after a codebase update.
---

# trust-code navigation (TRUST + TRIOCFD)

The indexes live in `.index/` (gitignored). They are flat, sorted text files
produced by GNU ctags (`ctags -x`), one line per symbol:

```
SYMBOL  kind  line  file  prototype...
```

## Build / rebuild

```bash
.opencode/scripts/index-codebases.sh            # both, skip if exists
.opencode/scripts/index-codebases.sh trust      # TRUST only
.opencode/scripts/index-codebases.sh trio       # TRIOCFD only
.opencode/scripts/index-codebases.sh both --force  # rebuild
```

Indexes only `src/` (headers + .cpp/.hpp/.cc). Run after `git pull` /
codebase updates. Build time is ~seconds even for the full kernel.

## Look up a symbol

```bash
.opencode/scripts/find-symbol.sh Objet_U
.opencode/scripts/find-symbol.sh associer_milieu_base --context 3
.opencode/scripts/find-symbol.sh Champ_Inc_base --context 3
.opencode/scripts/find-symbol.sh Foo --trio --count
```

Note: GNU ctags' 4th column holds the kind/return-type keyword (e.g. `class`,
`struct`, `void`, `#define`). `--kind` filters on that column, so it works for
`class`/`struct`/`#define` but not for "function" (use no filter and grep the
prototype instead).

Options: `--trust`/`--trio`/`--both`, `--kind KIND`, `--context N` (print N
lines around the definition), `--count`.

## Accuracy limits

- GNU ctags is syntactic, not semantic: heavy templates, macros, and
  generated code may be missed or mis-attributed.
- No caller/usages: ctags indexes definitions/declarations only. For usages,
  use ripgrep (`grep` tool) or `grep-codebase.sh`.
- If a lookup returns nothing, fall back to ripgrep over `src/` before
  concluding the symbol doesn't exist.

## Staleness

`find-symbol.sh` checks the index against the source repo's current git HEAD by
default and prints a warning to stderr if stale (disable with
`TRUST_INDEX_NO_CHECK=1`). `index-codebases.sh --check` reports status for
all/selected indexes (exit 1 if any stale/missing).

## Auto-refresh (opt-in)

Install post-commit/post-merge hooks in the source repos (run once per
machine):

```bash
.opencode/scripts/install-git-hooks.sh            # both
.opencode/scripts/install-git-hooks.sh --trust    # TRUST only
.opencode/scripts/install-git-hooks.sh --remove   # uninstall
```

Hooks are appended (not overwritten) and are idempotent. They are NOT
version-controlled, so re-run after a fresh clone.

## When to rebuild

- After updating trust-code or triocfd-code (or automatically via hooks).
- If `find-symbol.sh` reports "index missing" or "STALE".
- Indexes are gitignored, so each checkout builds its own (run once per
  machine/worktree).