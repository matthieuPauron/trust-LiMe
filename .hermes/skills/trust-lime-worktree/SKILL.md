---
name: trust-LiMe worktree workflow
description: >
  Git worktree usage for parallel workstreams in trust-LiMe: one workstream per
  worktree, one commit per session, and reclaiming accumulated worktrees. Use
  when starting parallel work or cleaning up worktrees.
---

# trust-LiMe worktree workflow

## Two repositories, two commits

`trust-LiMe` and `trust-code` are separate git repos. Never mix their changes
in one commit; commit each independently (kernel first when the module depends
on it).

## Creating a worktree

```bash
git worktree add ../trust-lime-<name> -b <name>
git worktree list
git worktree remove ../trust-lime-<name>
```

One workstream per worktree, one commit per session. Move the session into the
worktree when it becomes the primary working directory.

## Branches

- `main`/default holds the green baseline.
- Feature branches for multi-step work; merge only when the affected forms
  (or the full suite) are green.
- Keep experimental forms on a branch until validated.

## Reclaiming worktrees

```bash
hermes worktree   # audit and reclaim accumulated git worktrees
```

## Before committing (per worktree)

1. `git diff --check`
2. `.opencode/scripts/syntax-check.sh` on touched C++ files
3. `make module_semi_optim` if C++ changed
4. `git diff` review for `-Wconversion` hazards and stray debug code

## Nightly backlog usage

The nightly backlog job runs each task in its own worktree under
`/home/matthieu/Development/trust-LiMe-worktrees/<slug>` on branch
`nightly-YYYYMMDD-<slug>`. The main working tree stays clean; review each
worktree, then remove it with `git worktree remove <path>` (or `hermes
worktree`) after merging.

## Pitfalls

- Worktrees share the same `.git` object store but have independent working
  trees; do not run two heavy builds concurrently.
- Worktrees are not version-controlled state; re-run `install-git-hooks.sh`
  after a fresh clone.
- A worktree branch can collide on re-runs; append `-2`, `-3` when the branch
  or path already exists.
