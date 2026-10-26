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
per session). When a task directly follows the previous one (continuation,
refinement, or sub-task), **reuse the same worktree** instead of creating a
new one: make the transition explicit, execute the second task there, and
require validation for each task before the worktree is cleared.
```bash
# via git (Hermes has no opencode worktree command)
git worktree add ../trust-lime-<name> -b <name>
git worktree list
git worktree remove ../trust-lime-<name>
```

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

## Validation status file

`.opencode/validation_status.json` is the machine-readable validation status and
must be committed **every time it changes** (it is versioned evidence, not a
throwaway artifact). Whenever a validation run or `record-validation-status.py`
updates it, stage and commit it together with the change that produced it (or
in an immediately following `validation:` commit). Never leave it modified but
unstaged at the end of a session.

## Pre-commit hook

The repository's pre-commit hook runs the GoogleTest unit suite and may refresh
`.opencode/validation_status.json`; treat a failed hook as a real failure, not
a bypass. Re-run `git commit` after fixing.

Because the hook re-runs the tests and rewrites `validation_status.json`, a
commit that only updates that file will trip the hook and touch it again. Break
the loop by committing status-only changes with `git commit --no-verify`.
## In-repo Hermes skills (git tracking)

Project-local Hermes skills live under `.hermes/skills/` (or `.agents/skills/`). They are not discovered until the repo is trusted:

```bash
hermes skills trust   # run once per clone/worktree
```

Because this repo's `.gitignore` ignores the `trust*` basename, add an explicit
negation so project skills stay tracked (mirror the existing `.opencode/skills/trust*`
exception):

```gitignore
!.hermes/skills/trust*
```

When porting OpenCode skills (`.opencode/skills/`) to Hermes, copy them into
`.hermes/skills/`, replace OpenCode-only slash commands (`/syntax`, `/build`,
`/validate`, `/review`) with the underlying scripts/commands, add a short
"Hermes usage" note pointing at `terminal`/`read_file`/`patch`, and keep the
OpenCode originals in place. Commit the two trees separately so each runtime's
skill set evolves independently.

## Reviewing

For a numerics-focused review of the current diff, run `git diff` and review
blockers / important / nits with `file:line` (or load `requesting-code-review`).


## Hermes usage

In Hermes there are no OpenCode slash commands. Run the scripts above via the
`terminal` tool (paths are repo-relative), read/edit files with `read_file` /
`patch` / `write_file`, use `execute_code` for bulk checks, and `delegate_task`
for parallel analysis. Heavy builds must pass `.opencode/scripts/check-heavy-build.sh`
and follow the RAM guard in `AGENTS.md`.


**Exception:** if the only change is the `unit_tests` block or the `updated`
timestamp produced by the pre-commit hook (unit-test results), do **not**
commit it — revert with `git checkout -- .opencode/validation_status.json`
(or `git restore`). Those are hook noise, not a validation-state change worth
versioning. Only commit validation_status.json when a real validation run,
`record-validation-status.py` (non-unit), or a form result changed it.
