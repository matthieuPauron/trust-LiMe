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

### Integrating worktree work into `main` (preferred)

Keep `main` linear and free of `nightly-YYYYMMDD-<slug>` merge commits: rebase
the worktree branch onto the current `main` tip, resolve conflicts, then
fast-forward `main` to the rebased tip:

```bash
cd <worktree>
git fetch origin main 2>/dev/null || true
git rebase main          # replay worktree commits on top of main
# resolve conflicts, git add ..., git rebase --continue
cd <main-repo>
git merge --ff-only <branch>   # fast-forward only; never create a merge commit
git worktree remove <worktree>
git branch -D <branch>
```

Never `git merge` (no-ff) worktree branches into `main`; if a fast-forward is
not possible, rebase again until it is.

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

## Commit hygiene (readability)

The git history must stay readable. Follow these rules for every commit:

1. **Squash status refreshes.** `validation: refresh status` commits are noise —
   they only rewrite `validation_status.json`. Never create a standalone refresh
   commit. Fold the status update into the commit that produced the validation
   result, or accumulate several and commit once per session with
   `git commit --amend` / interactive rebase. When cleaning history, mark the
   refresh commits as `fixup` (squash into predecessor, discard message).
2. **Conventional subjects.** Use `type(scope): summary` with a lowercase type:
   `feat`, `fix`, `docs`, `test`, `build`, `chore`, `refactor`, `merge`,
   `validation`. Prefer the repo's existing types (`validation:`, `skill:`,
   `backlog:`, `nightly backlog:`) over inventing new ones. Avoid capitalized
   subjects (`Add …`, `WIP: …`) and vague ones (`update`, `refresh`).
3. **One logical change per commit.** Do not mix code wiring + status refresh +
   `NEXT.md` reorder in one commit. Split: code commit, then docs/status commit.
   Never bundle unrelated hunks with `git add -A`.
4. **`validation_status.json` is not a routine commit.** It is versioned evidence:
   commit it only on a validated milestone, alongside the evidence (notebook/data)
   that justifies the change. Hook-only rewrites of the `unit_tests` block or
   `updated` timestamp are noise — revert with `git checkout -- .opencode/validation_status.json`.
   Only commit it when a real validation run, `record-validation-status.py`
   (non-unit), or a form result changed it.
5. **Clean up before pushing.** Run `git rebase -i` to squash consecutive
   `validation: refresh status` commits, drop empties, and reword vague subjects.
   Never rewrite history that has already been pushed/shared without confirmation.

## Validation status file

`.opencode/validation_status.json` is the machine-readable validation status and
is versioned evidence, not a throwaway artifact. Commit it only on a validated
milestone, together with the change that produced it (or in an immediately
following `validation:` commit). Never leave it modified but unstaged at the end
of a session, but do not create standalone `validation: refresh status` commits
for it (see Commit hygiene above).

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

## Heavy files (Git LFS)

TRUST versions heavy validation meshes and binaries through **Git LFS**
(`trust-code/.gitattributes` tracks `*.med`, `*.msh`, `*.gz`, `*.geom`,
`*.cgns`, `*.sauv`, `*.tgz`, `*.zip`, `*.xyz`, images, …). trust-LiMe has no
LFS config yet. A committed `.med`/`.msh` without LFS bloats the repo and
history.

- **Preferred for validation cases:** generate the mesh with the SALOME script
  (e.g. `share/.../VKSLite/geometry/salome_vks_spherical_shell.py`) and commit
  only the script — reproducible, zero repo bloat. This is what most TRUST
  cases do.
- **If the binary mesh must be versioned:** add a `.gitattributes` mirroring
  TRUST's heavy-file patterns and run `git lfs track` + `git lfs install`
  before committing. Git LFS is installed (3.4.1). Do not commit a raw `.med`
  without LFS.
- Never mix a large binary into a normal commit; check `git diff --stat` for
  size before pushing.

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
