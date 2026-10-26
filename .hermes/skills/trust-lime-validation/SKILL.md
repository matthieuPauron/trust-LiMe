---
name: trust-lime-validation
description: "Author and run trust-LiMe validation forms and 3D twins."
version: 0.1.0
author: Hermes Agent
license: MIT
platforms: [linux]
metadata:
  hermes:
    tags: [trust-lime, validation, notebooks, 3d-twin]
    related_skills: [trust-lime-build-diagnostics, trust-lime-plan-management]
---

# trust-LiMe validation

## When to use

- Adding or modifying a validation form under `share/Validation/Rapports_automatiques/`.
- Creating a 3D twin of an existing 2D coupled/standalone form.
- Running or debugging a notebook/dataset.

## Creating a 3D twin (coupled forms)

1. Create `share/Validation/Rapports_automatiques/MagneticCoupled3D/<Name>3D/` with `src/<name>_3d.data` and `<Name>3D.ipynb`.
2. Start from the 2D dataset; change `dimension 2` -> `3`, add the third axis to `Pave`/borders, add `Polyedriser dom` after `Mailler` (PolyMAC needs polyhedra in 3D), set `nb_composantes 3`, and add the missing boundary faces (devant/derriere).
3. Keep the analytic solution z-independent where possible; probes at `z = 0.5` (mid-plane) reuse the 2D probe positions.
4. Add the form to `parallel_ok[]` in `.opencode/scripts/check-validation.sh` if it uses partition machinery, then run `check-validation.sh --all`.
5. Copy a simple 3D notebook (e.g. `ShercliffDuct3D`) and adapt probes; run it headless with the TRUST env sourced and `$exec` pointing at the trust-LiMe module.

## Reading TRUST probes (`.son`)

- Use `SonFile(path, None)` (the single-arg form is not supported), then `son.getEntries()` and `son.getValues(entry)`.
- Probe files land in `src/` next to the dataset; copy them into `BUILD_DIRECTORY` after `runCases()` or read from `src/`.
- Effective probe coordinates come from the `# Temps ... x= ... y= ...` header; evaluate analytic solutions there, not at nominal coordinates.

## Dataset comment markers (parallel forms)

- `# BEGIN MESH #` is a closed comment on one line (MESH block stays active in nominal mode).
- `# BEGIN PARTITION` and `# BEGIN SCATTER` must stay **open** (no trailing `#`) so the block is commented out in nominal mode and only activated by `make_PAR.data`/`trust -partition`.
- Never bulk-close these markers with a global sed: it activates partitioning and can skip the solve.

## Validation status file

- `.opencode/validation_status.json` is versioned evidence: commit it every time it changes (validation run or `record-validation-status.py`). Never leave it modified-but-unstaged.

## In-repo Hermes skills

- Project-local Hermes skills live in `./.hermes/skills/` (not `.opencode/skills/`).
- Gitignore exception required: add `!.hermes/skills/trust*` alongside the existing `.opencode/skills/trust*` rule, otherwise `trust*` ignores them.
- Trust the repo once with `hermes skills trust` so the skills load in sessions started inside it.

## Pitfalls

- In 3D, `Polyedriser dom` must come right after `Mailler` (PolyMAC needs polyhedra).
- A 3D form needs all six boundary faces defined in BOTH the Navier-Stokes and magnetic blocks; missing front/back faces break meshing or the solve.
- `check-validation.sh` defaults to git-tracked files; use `--all` to scan new untracked forms. Re-run it after editing `parallel_ok[]`.
- Heavy builds (sanitizer/full-kernel) follow the RAM guard in `AGENTS.md` and `.opencode/scripts/check-heavy-build.sh`.
- The nightly backlog job runs each task in its own worktree under `trust-LiMe-worktrees/`; changes are NOT in the main tree. Validate by inspecting the worktree paths in `nightly-backlog-state.json` (`worktrees`), then remove the worktree after merge.
- A dataset using `$nx`/`$ny`/`$nz` placeholders is a template: the notebook must call `run.addCaseFromTemplate(..., dic={...})`. Calling `run.addCase(".", ...)` leaves the placeholders in the generated `DEC_`/`PAR_` files and fails with `Error converting a string to type int: $nx`.
- In 3D, keep the analytic solution z-independent where possible and place probes at the mid-plane (`z = 0.0` or `0.5` matching the mesh); always read effective coordinates from the `.son` header rather than nominal ones.
- `SonFile(path)` (single argument) is not supported; use `SonFile(path, None)` then `getEntries()`/`getValues()`.
