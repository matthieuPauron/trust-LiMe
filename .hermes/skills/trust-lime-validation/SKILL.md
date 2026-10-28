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
- Generated artifacts (`build/` dirs, `.son`/`.sons`/`.TU`/`.stop` files) under `share/Validation/Rapports_automatiques/` are gitignored and are deleted automatically by `run-validation-pdf.sh` after `trust-LiMe_validation.pdf` is produced. Do not commit them; do not hand-clean them mid-run.

## In-repo Hermes skills

- Project-local Hermes skills live in `./.hermes/skills/` (not `.opencode/skills/`).
- Gitignore exception required: add `!.hermes/skills/trust*` alongside the existing `.opencode/skills/trust*` rule, otherwise `trust*` ignores them.
- Trust the repo once with `hermes skills trust` so the skills load in sessions started inside it.

## Developing / inertial flow benchmarks

- A finite-duct case with a **plug (uniform) inlet** and an **open outlet carrying the Hartmann pressure profile** exercises `u·∇u ≠ 0` — the regime the fully-developed `HartmannTwoWay` case cannot (there `u·∇u = 0`).
- Inlet: `frontiere_ouverte_vitesse_imposee` with a uniform profile; outlet: `frontiere_ouverte_pression_imposee` with the Hartmann magnetic-pressure profile (`-0.5*(sinh(10*y)/742.099-y)^2`). Leave the inlet pressure floating — do not impose it.
- The steady state develops from plug toward Hartmann. The Hartmann reference centreline is `u_H = G h^2/(μ Ha^2)(1 - 1/cosh Ha)` with `G` from the mass flow `M = 2h U_plug`: `G = M μ Ha^2/(2 h^3 (1 - tanh(Ha)/Ha))`. Compare outlet probes to this reference.
- Mass flow is conserved, so the Hartmann centreline (~1.23 × plug velocity at Ha=5) is **higher** than the plug velocity: the flow **accelerates**, it does not decelerate. A benchmark asserting "outlet < inlet" would be physically wrong.
- Mesh-convergence: use `run.addCaseFromTemplate` with `$nx`/`$ny` placeholders over coarse/standard/fine; read effective probe positions from the `.son` header (`probe_xy`) because `grav` snaps to cell centres and positions shift with refinement.
- Assert **outlet velocity convergence** (std→fine diff below tolerance) rather than a fixed development ratio, which stays mesh-dependent.

## Gauss-law projection (`correction_divergence`)

- Dirichlet support is implemented as a **constrained projection**: Dirichlet boundary faces are excluded from the Jacobian `J` (their trace is a constant) and their prescribed `b0` enters `D(b)` as a fixed term, so prescribed boundary values are never overwritten. See `docs/content/plans/gauss_law_dirichlet.md`.
- `mettre_a_jour()` must call `build_bc_trace_tables(temps)` before `corriger_divergence()` so time-dependent Dirichlet CLs use current prescribed values; `assembler_correction()` caches `L` (type-only, no value refresh needed).
- `gradient_b_elements()` detects Dirichlet CLs independently (`is_dirichlet_face`) and uses prescribed values for the reported divergence; keep it consistent with `bc_trace`.
- Kernel unit test: mark boundary faces as `BC_DIRICHLET` with prescribed values; verify (a) divergence uses prescribed trace, (b) net flux uses prescribed value, (c) Jacobian is reduced, (d) correction is finite.
- **Form-level (Dirichlet wall):** prescribed Dirichlet fluxes must be **compatible** (net boundary flux ≈ 0) or the run aborts with `Gauss law violated by boundary conditions`. Opposite walls need the same normal component (e.g. bottom and top both `b=(0,1)`), not opposite signs.
- **Dirichlet CL syntax:** use `temperature_imposee_paroi` (thermal CL reused because `domaine_application() == \"Thermique\"`) with a `Champ_Front_Uniforme` carrying `nb_composantes` values, e.g. `bas temperature_imposee_paroi Champ_Front_Uniforme 2 0. 1.`. The CL is a `Dirichlet` subclass, so `build_bc_trace_tables` detects it automatically.
- **`divergence_magnetique` post-treatment field requires `nb_composantes == dimension`** (full vector field); with fewer components `creer_champ` calls `Process::exit()`. Use `nb_composantes 2` in 2D / `3` in 3D when probing divergence.

## Merging validation into an existing notebook

- Prefer folding a new validation into an existing sibling notebook over creating another `<FormName>.ipynb`. Extra notebooks proliferate; a single notebook per physical topic keeps the reportable suite navigable.
- Steps:
  1. Move the new case's `.data` into the target form's `src/` (e.g. `GaussLaw/src/gauss_law_dirichlet.data`).
  2. Append cells to the target `.ipynb` via JSON (never hand-edit markdown): a `run.reset()`, an `addCaseFromTemplate(...)` + `runCases()`, a probe-read/assert cell, and a `run.tablePerf()`. Build the cell list in Python and `json.dump` with `indent=1`.
  3. Validate by running the appended section headless: create a temporary notebook containing only those cells plus the `TRUST_parameters`/`reset` setup, place it in the form dir, run `jupyter nbconvert --to notebook --execute` with `env_trust_LiMe.sh`, `env_for_python.sh`, and `venv_trust/bin/activate` sourced, then delete the temp notebook.
  4. Remove the now-redundant standalone form directory (notebook + build + checkpoints) but keep its `src/*.data` (moved, not deleted).
  5. Drop the stale entry from `.opencode/validation_status.json` (the form no longer exists).
- Keep the assertion that captures the actual physics being validated; do not weaken it to make the merge pass.
- `run.reset()` in the appended section is safe because prior sections have already asserted their results; it only clears case state for the new run.

## Pitfalls"

- In 3D, `Polyedriser dom` must come right after `Mailler` (PolyMAC needs polyhedra).
- A 3D form needs all six boundary faces defined in BOTH the Navier-Stokes and magnetic blocks; missing front/back faces break meshing or the solve.
- `check-validation.sh` defaults to git-tracked files; use `--all` to scan new untracked forms. Re-run it after editing `parallel_ok[]`.
- TRUST `.data` comments must be **closed on the same line** (`# ... #`): an opening `#` with no matching close makes the parser treat the following words as keywords (error: `Error while reading 'X,'`). This includes header/comment blocks — safest is to keep comments minimal or fully closed.
- Boundary-condition fields go on the **next line**, not inline: `inlet frontiere_ouverte_vitesse_imposee` then `Champ_front_Uniforme 2 1. 0.` on the following line. Case matters (`Champ_front_Uniforme`, not `Champ_front_uniforme`).
- A new form's notebook must be **valid JSON** (`.ipynb`). Build it with `json.dump` rather than hand-writing markdown; run it headless with `jupyter nbconvert --to notebook --execute` after sourcing `env_trust_LiMe.sh`, `env_for_python.sh`, and `venv_trust/bin/activate`.
- Heavy builds (sanitizer/full-kernel) follow the RAM guard in `AGENTS.md` and `.opencode/scripts/check-heavy-build.sh`.
- The nightly backlog job runs each task in its own worktree under `trust-LiMe-worktrees/`; changes are NOT in the main tree. Validate by inspecting the worktree paths in `nightly-backlog-state.json` (`worktrees`), then remove the worktree after merge.
- When a task directly follows the previous one, the job reuses the same worktree (entry lists `tasks: [title1, title2, ...]`). Validate each task before the worktree is cleared; do not treat the worktree as validated until all tasks in it are validated.
- A dataset using `$nx`/`$ny`/`$nz` placeholders is a template: the notebook must call `run.addCaseFromTemplate(..., dic={...})`. Calling `run.addCase(".", ...)` leaves the placeholders in the generated `DEC_`/`PAR_` files and fails with `Error converting a string to type int: $nx`.
- In 3D, keep the analytic solution z-independent where possible and place probes at the mid-plane (`z = 0.0` or `0.5` matching the mesh); always read effective coordinates from the `.son` header rather than nominal ones.
- `SonFile(path)` (single argument) is not supported; use `SonFile(path, None)` then `getEntries()`/`getValues()`.


**Exception:** if the only change is the `unit_tests` block or the `updated`
timestamp produced by the pre-commit hook (unit-test results), do **not**
commit it — revert with `git checkout -- .opencode/validation_status.json`
(or `git restore`). Those are hook noise, not a validation-state change worth
versioning. Only commit validation_status.json when a real validation run,
`record-validation-status.py` (non-unit), or a form result changed it.
