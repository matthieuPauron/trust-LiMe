---
name: trust-LiMe git workflow
description: >
Git hygiene for trust-LiMe: grouping changes into coherent commits, branch and
worktree usage, reviewing diffs, and committing kernel (trust-code) vs module
(trust-LiMe) changes separately. Use when staging, committing, creating
branches/worktrees, or reconciling uncommitted work across the two repos.
---

# trust-LiMe git workflow

## Two repositories, two commits

`trust-LiMe` (this repo) and `trust-code` (sibling kernel checkout) are
separate git repos. Never mix their changes in one commit. Commit each repo
independently, kernel first when a module change depends on a kernel change.

## Coherent commits

Group by *intent*, not by file count. A good commit:
- touches one logical change (one feature, one fix, one refactor);
- builds and passes `syntax-check.sh` / `make module_semi_optim`;
- has a message stating the *why*, not the *what*.

Use `git status --short` + `git diff --stat` to survey, then stage with
`git add -p` (or pathspecs) to split hunks. Avoid `git add -A` when unrelated
changes are present.

## Worktrees

Use worktrees for parallel workstreams (one workstream per worktree, one commit
per session):

```bash
# via opencode tooling
opencode worktree create <name>
opencode worktree list
opencode worktree remove <dir> [--force]
```

Move the session into the worktree with `opencode session move <dir>` when the
worktree becomes the primary working directory.

## Branches

- `main`/default branch holds the green baseline.
- Feature branches for multi-step work; merge only when the validation suite
  (or at least the affected forms) is green.
- Keep experimental forms (e.g. `MagneticTurbulence/`) on a branch until
  validated.

## Before committing

1. `git diff --check` (no whitespace errors).
2. `.opencode/scripts/syntax-check.sh` on touched C++ files.
3. `make module_semi_optim` if C++ changed.
4. `git diff` review for `-Wconversion` hazards and stray debug code.

## Reviewing

For a numerics-focused review of the current diff, use the `/review` command
(blocker / important / nit, with `file:line`).