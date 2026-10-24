---
name: trust-LiMe notebooks
description: >
Author, edit, and run trust-LiMe Jupyter validation notebooks and their .data
datasets (share/Validation/Rapports_automatiques/): trustutils pipeline
(addCase/runCases/tablePerf), dataset syntax rules, headless nbconvert run
recipe, baseline/RULE harness, and the sequential-by-default execution rule
(with the documented parallel exception). Use when touching any .ipynb or .data
file, or running validation. Test-tier concepts live in trust-lime-testing.
---

# trust-LiMe notebooks & datasets

The notebook layout follows the official TRUST 1.9.8 pipeline:
`Rapports_automatiques/<Case>/<Case>.ipynb` + `src/*.data`, driven by
`trustutils.run`.

## Validation-case formatting rules (hard requirements)

Four rules, R1–R3 enforced mechanically by
`.opencode/scripts/check-validation.sh` — run it before `/validate` (exit
0 = clean). R4 is the design rule that governs how notebooks are split:

- **R1 — notebook placement.** Every notebook is a *validation form* and lives
  at the form root, following the official TRUST layout
  (`Validation/Rapports_automatiques/<Category>/<FormName>/`):
  `share/Validation/Rapports_automatiques/<Category>/<FormName>/<FormName>.ipynb`
  — the notebook is named after its own folder, and its datasets sit in
  `<FormName>/src/*.data` (never at `MagneticStandalone/<FormName>.ipynb` with
  a shared `src/`). For a simple case that is its own form this reduces to
  `<Case>/<Case>.ipynb`. Never put an `.ipynb` in `src/` (nor deeper;
  `build/` and `.ipynb_checkpoints/` are generated and git-ignored).
- **R4 — one form per notebook.** A notebook holds the cases of **one
  validation form** (its own folder): the baseline dataset plus additional
  runs of that same form — typically its **mesh-convergence scan levels**,
  generated from a template `src/<case>.tpl.data` via
  `run.addCaseFromTemplate(..., excluNR=True)`. Never bundle datasets of
  *different* forms in one notebook: every addCase'd dataset must be local to
  the form (`run.addCase(".", ...)`), and template target directories stay
  inside `build/`. Consequences:
  - one `.ipynb` per form, named after the form
    (`MagneticStandalone/{Diffusion,Advection,…}/`), possibly running the
    same case at several mesh levels;
  - shared code (imports, probe helpers) is duplicated in each notebook —
    keep it small and identical so a reader can run any notebook standalone;
  - the mesh-convergence scan is part of the form's notebook (see
    "Mesh-convergence scan" below) — the baseline case keeps its exact
    dataset, case name, probes and NR identity;
  - a new ASN 28 category does **not** justify a second form in a notebook —
    it justifies a second notebook. A notebook still holds cases of one
    single ASN 28 category.
  - **a 2D form may carry its 3D twin as a second dataset** in the same form
    folder (`MMS_Coupled` runs `mms_coupled.data` + `mms_coupled_3d.data`
    since 2026-10-21, retiring the separate `MagneticCoupled3D/MMS_Coupled3D`
    form). The two ladders are independent, and every case needs its **own**
    build subfolder (`build/<dataset>/<scale>/`) so the partitioned
    `DOM_000*.Zones` cannot collide — see "Mesh-convergence scan" below.
- **R5 — 2D/3D twins.** Every standalone-magnetic form exists in 2D
  (`MagneticStandalone/<FormName>/`) and in 3D (`MagneticStandalone3D/<FormName>/`,
  same form name, same assertions). A 3D dataset differs from its 2D
  counterpart only by `dimension 3`, three-component vector/probe
  coordinates, six borders, and **`Polyedriser dom` right after `Mailler`**
  (PolyMAC_MPFA requires polyhedral cells in 3D). Keep twins in sync when a
  case changes. (The `Case` folder is the ASN 28 category grouping.)
- **R2 — sequential-only datasets (default).** A `.data` file **must not
  contain any TRUST parallelism machinery**: `Decouper`, `Partitionneur`,
  `Partition`, `Partition_tool`, `Scatter`, `Larg_joint`, `Nom_Zones`,
  `zones_name`, nor the `# BEGIN PARTITION #` / `# BEGIN SCATTER #` section
  markers. Keep meshes self-contained in the dataset (`Mailler`/`Pave`), not
  in partitioned files (`*.Zones`). If you copy a kernel dataset as a
  template, strip all parallel machinery before adapting it.
  **Exception:** paths listed in `parallel_ok[]` in `check-validation.sh`
  are allowed to carry the parallel machinery (see "Parallel exception"
  below). Add a path there deliberately — never disable R2/R3 globally.
- **R3 — sequential-only notebooks (default).** Never `case.partition()`, never
  `nbProcs > 1`, never `trust -partition` on the command line.
  **Exception:** the same `parallel_ok[]` paths.

## Execution rule (sequential by default)

**trust-LiMe test cases run sequentially unless explicitly allowlisted.** Always:

- `run.addCase(".", "case.data")` — no `nbProcs`, or `nbProcs=1`
  explicitly; never `case.partition()`.
- Never a processor count on the `trust` command line.

```bash
.opencode/scripts/check-validation.sh   # R1-R4; exit 0 = clean
```

### Parallel exception

A case may run on `N` cores only if its path is listed in `parallel_ok[]` in
`check-validation.sh`. The dataset must then carry the three comment-marker
regions that `trust -partition` toggles (upstream convention, see
`Verification/PolyMAC_MPFA/grad_rot/src/grad_rot.data` in trust-code):

```c
# PARALLEL OK #                       <- rewritten to '# PARALLEL ONLY N #' by make_PAR.data

# BEGIN MESH #                        <- nominal: self-closed, mesh body ACTIVE
Mailler dom { ... }
# END MESH #                          <- PAR: pair becomes a skip region (mesh read from .Zones)

# BEGIN PARTITION                     <- nominal: body commented; DEC: active
Partition dom
{
    Partition_tool metis { nb_parts 2 }
    Larg_joint 2
    zones_name DOM
}
End
END PARTITION #

# BEGIN SCATTER                       <- nominal: inert; PAR: Scatter becomes active
Scatter DOM.Zones dom
END SCATTER #
```

and the notebook:

```python
NB_PROCS = 2
case = run.addCase(".", "case.data", nbProcs=NB_PROCS)
case.partition()      # trust -partition case.data 2 -> PAR_case.data + DOM_000*.Zones
run.runCases()        # trust PAR_case 2
```

Traps:

- After `partition()`, the dataset (and every output file) is named
  `PAR_<case>` — read probes as `read_son(case.dataFileName_, ...)`.
- `#` is a **token-level open/close pair** in the data-file lexer
  (`src/Kernel/Utilitaires/LecFicDiffuse_JDD.cpp`), not a line comment. A
  `# … #` line inside a region that `make_PAR.data` turns into a skip-region
  (the MESH / PARTITION blocks) closes the skip early and desynchronises the
  whole file — use `/* … */` there instead (as upstream `Obstacle.data` does).
- In 3D, `Polyedriser dom` goes **inside** `# BEGIN MESH # … # END MESH #`
  (the polyhedra must come from the pre-partitioned `.Zones`).
- trustutils' *suite* sequential mode is unaffected: `nbProcs` sets the number
  of ranks per case, not suite concurrency.

## Notebook skeleton (trustutils)

```python
from trustutils import run

run.introduction("Author", "date")
run.TRUST_parameters("1.9.8")

run.reset()
run.addCase(".", "my_case.data")          # sequential, no nbProcs
run.printCases()
run.runCases()
run.tablePerf()                           # always the last cell
```

- Use `run.addCaseFromTemplate("case.data", targetDirectory=..., dic={...})`
  for mesh-refinement / parameter sweeps — see "Mesh-convergence scan" below.
- Read results from `run.BUILD_DIRECTORY/<dataset name>/` — never
  hardcode `./build`.

## Mesh-convergence scan (every non-negative form)

Every validation form that actually solves carries a mesh-convergence scan
in its own notebook. The **RULE** governs any notebook edit: an operation is
validated only if the notebook's existing computations reproduce exactly —
enforced by `.opencode/scripts/notebook-baseline.sh execute|compare`
(snapshot → edit → re-run → compare outputs + probe values).

- **Baseline = the coarse scale.** The historical baseline dataset *is* the
  `coarse` entry of the ladder: same case name, same probes, same NR
  identity, same mesh — now instantiated through the same template as the
  other scales instead of a separate `addCase(".")`.
- **One template dataset.** `src/<case>.data` **is** the trustutils
  template: its `Nombre_de_Noeuds` values are Python-`Template` keys
  (`$nx $ny [$nz]`); the `# PARALLEL OK #` and the three
  `# BEGIN MESH/PARTITION/SCATTER` markers are kept verbatim. There is no
  `.tpl.data` twin any more — one data file, substituted per scale.
- **Scale naming.** `coarse` = the baseline mesh, `standard` = cells ×2
  (`nodes N → 2N-1`), `fine` = cells ×4 (`nodes N → 4N-3`), applied to the
  directions the solution varies in (quasi-1D forms refine the active
  direction only; genuinely 3D fields refine everything). Never use
  `L1`/`L2` — they read like the L¹/L² metric spaces.
- **Registration** — the whole Run cell registers the three scales; the
  **same data-file name** runs in every scale (the case name *is* the data
  file name), one build subfolder per scale:

  ```python
  NX0, NY0 = ..., ...                    # baseline node counts (coarse)
  meshes = {
      "coarse":   {"nx": NX0,     "ny": NY0},        # the baseline mesh
      "standard": {"nx": 2*NX0-1, "ny": 2*NY0-1},    # cells x2 -> h/2
      "fine":     {"nx": 4*NX0-3, "ny": 4*NY0-3},    # cells x4 -> h/4
  }
  cases = {}
  for lvl, dic in meshes.items():
      cases[lvl] = run.addCaseFromTemplate(f"{CASE}.data", lvl, dic=dic,
                                           nbProcs=NB_PROCS, excluNR=True)
      cases[lvl].partition()      # one DOM_000*.Zones set PER SCALE DIR
  ```

- **Probes.** Probes land next to their dataset:
  `build/<scale>/PAR_<case>_<sonde>.son` (and
  `build/<Case>/<scale>/…` for the merged multi-case MMS forms), plus the
  build root for plain `addCase(".")` datasets. `read_son()` looks in
  `build/coarse/` (and `build/<Case>/coarse/`) first, then the build root.
  Read the *effective* probe coordinates from the `.son` header
  (`# Temps x= … y= …`) and evaluate the analytic solution there — the
  metric is then mesh-independent; `coarse` reuses the baseline probes
  unchanged.
- **Zones locality.** Every partitioned case must live in its own build
  subfolder: `trust -partition` writes `DOM_000*.Zones` **in the case's
  directory**, and several root-level cases would silently overwrite each
  other's zones (all solves would then run on the *last* partition's mesh —
  the M4 merge-era MMS notebooks shipped with exactly this bug).
- **Output.** A compact table only (scale, nodes, h, error, observed order
  `log2(e_k/e_{k+1})`) before the Computer Performance cell. Assert:
  strict error decrease across scales for analytic-solution forms; error
  below the form's existing tolerance at every scale for invariance /
  machine-zero forms. The observed order is printed, never asserted.
  Negative (`ErrorInputs`) forms get no scan (the abort is
  mesh-independent) — a one-sentence markdown note says so.

### Hard rules (TRUST upstream)

- Never import `matplotlib` or `trustutils.plot` **before** `runCases()`
  — it breaks non-regression extraction. Import them in the analysis
  cells after.
- Custom files produced by a cell need `run.saveFileAccumulator()`.
- For PDF export, embed images as markdown `![](src/fig.png)`, not HTML.
- A notebook must contain cases from **one ASN 28 category** (see
  `trust-lime-testing` skill and `docs/content/test_cases.md`).

### Post-processing API

- Probes: `plot.Graph(...).addPoint(son_file)` / `.addSegment(son_file)`
- Residuals: `.addResidu(dt_ev_file)`
- Probe extraction: `run.extraitCoupe(data_file, probe_name, ...)`
- VisIt/lata: `trustutils.visit` (`showMesh`, `showField`,
  `export_lata_base`) — always guard with `if not
  visit.VisItDisabled():`.

### Probe file placement

After `runCases()`, the individual `.son` probe files are created by TRUST
in `src/` (next to the `.data` file), e.g.:

  `share/Validation/Rapports_automatiques/MagneticStandalone/src/advection_SONDE_X025.son`

The notebook's `read_son()` function expects them in `BUILD_DIRECTORY`:

  `path = f"{BUILD}/{case}_{name}.son"`  →  `/home/.../build/advection_SONDE_X025.son`

Since TRUST does **not** automatically place them there, add this copy step
after `runCases()` and before the analysis cells:

```python
import shutil, pathlib
src_dir = pathlib.Path("src")
build_dir = pathlib.Path(run.BUILD_DIRECTORY)
for son in src_dir.glob(f"*{case}*.son"):
    shutil.copy2(son, build_dir / son.name)
```

Or, alternatively, use `run.extraitCoupe()` to extract probe data programmatically
instead of relying on the `.son` files.

### Non-regression mechanics

Every `addCase`'d case becomes an NR test truncated to 3 timesteps.
Override with `# NB_PAS_DT_MAX n #` inside the `.data`; exclude with
`excluNR=True`. Python `pre_run`/`post_run` callables hook cases (pre_run
participates in NR extraction; post_run doesn't).

## Dataset (.data) syntax gotchas

- Comments are `# ... #` — **opened AND closed**, `#` surrounded by
  blanks. `"# bla"` alone is NOT a comment. `/* ... */` also works;
  `//` does not exist.
- Braces and commas need surrounding whitespace: `Read_MED { ... }`,
  not `Read_MED{...}`.
- Not a programming language — no loops or conditionals.
- Keywords are case-insensitive; object identifiers are case-sensitive.
- `Solve my_problem` is mandatory — without it nothing runs.
- Canonical order: `Dimension` → problem → `Domaine`/`Mailler` →
  discretization object → time scheme → `Associate` → `Discretize` →
  `Read pb { ... }` → `Solve`.
- Medium since 1.9.1: `gravity Uniform_field <dim> gx gy` sits **inside**
  the `Fluide_Incompressible { ... }` block.
- Restart: adding/removing an equation on restart requires `.xyz` saves,
  not `.sauv` (same-equation-set only). Use `resume_last_time binaire`.
- If the dataset rejects `Convection_Diffusion_Magnetic_field` as
  unknown, the module isn't linked/registered — check the build, not the
  syntax.
- Fast syntax examples for any keyword: `trust -search "<keyword>"`.

## Running a notebook (headless)

Must run in the **TRUST Python environment**, not the OpenCode venv.

**The executable MUST be the trust-LiMe module binary**, not the plain TRUST
kernel. trustutils picks its binary from the `$exec` environment variable;
only `env_trust_LiMe.sh` sets it to `trust-lime_opt`. If you run without it,
`$exec` points at the kernel and every case fails with
`PB_MHD is not a recognized keyword`. **Always verify before executing:**

```bash
[ -n "$exec" ] && [[ "$exec" == *trust-lime* ]] || { echo "WRONG exec: $exec"; exit 1; }
```

```bash
source ../trust-code/env_TRUST.sh
source ../trust-code/env_for_python.sh
source ~/Development/venv_trust/bin/activate
source env_trust_LiMe.sh   # REQUIRED — see below
jupyter nbconvert --to notebook --execute \
  --output-dir /tmp/opencode/trust-lime-notebooks \
  share/Validation/Rapports_automatiques/<Case>/<Case>.ipynb
```

Before re-running, clean stale artifacts in the case src/ directory
(`.stop`, `.out`, `.err`, `*.son`, `*.TU`) — a leftover `.stop` stops the
solve early. Use a generous timeout; summarize results; on a cell error
show the traceback and stop.

## PDF export & the repository report

Every form can be exported to PDF and all forms concatenated into a **single
validation report at the trust-LiMe repository root**
(`trust-lime_validation.pdf`). Rule: **never commit per-form PDFs or
the concatenated report** — they are generated artifacts (like `build/`) and
stay untracked.

```bash
# per form: execute, then export to PDF (trustutils plot images are embedded
# already; use markdown "![](…)" not HTML for any custom figure)
jupyter nbconvert --to notebook --execute --output-dir /tmp/opencode/lime_pdf/notebooks <Form>/<Form>.ipynb
jupyter nbconvert --to pdf --output-dir /tmp/opencode/lime_pdf/pdf <executed>.ipynb

# concatenate into one report at the repo root (the runner visits the four
# categories in order — MagneticStandalone, MagneticStandalone3D,
# MagneticCoupled, Postraitement_fields — and names each per-form PDF
# <Category>_<FormName>.pdf)
pdfunite /tmp/opencode/lime_pdf/pdf/*.pdf "$repo/trust-lime_validation.pdf"
```

- The full pipeline (execute → PDF → `pdfunite`) is
  `.opencode/scripts/run-validation-pdf.sh` — run it from the repo root with
  the TRUST Python env sourced (see "Running a notebook (headless)"), after
  `check-validation.sh` is green.
- **Inline math in a table cell must close without a space before `$`**
  (write `$\nabla\times b$`, never `$\nabla\times b $`): pandoc mis-parses the
  trailing space, emits the macros in text mode and XeTeX aborts with
  "Missing $ inserted" (first hit by the pre-merge `DivergenceMagnetique`
  form, now part of `Champs2D`).
- The concatenated report is named after the repository
  (`trust-lime_validation.pdf`) and covers every category; add a new category
  to the runner's `cats` list when a form lands there. Negative (diagnostic)
  forms end with a summary print instead of
  `run.tablePerf()` — they run `trust` directly and have no suite; do not add
  `run.tablePerf()` to them (it crashes on `defaultSuite_ == None`).
- Trigger the same pipeline from `.opencode/scripts/after-trust-optim.sh`
  (step 4) so a full optim build ends with the report regenerated.

## Baseline / non-regression (RULE harness)

Notebook operations are gated by the RULE harness:

```bash
.opencode/scripts/notebook-baseline.sh execute <FormName>   # run + record baseline
.opencode/scripts/notebook-baseline.sh compare  <FormName>   # verify unchanged cells
```

Unchanged cells must print identically and baseline probes must be reproduced
exactly. Run the gate before `/validate`.

## Running a single case with `trust` (Run_fiche)

For a quick single-case run (no notebook), use the TRUST runner:

```bash
cd share/Validation/Rapports_automatiques/<Category>/<FormName>
source .../env_TRUST.sh  (or use with-trust-env.sh)
$TRUST_ROOT/bin/trust <case>.data 2>&1 | tail -15
```

Or `Run_fiche -export_pdf` for a full PDF. Always source the env first (see
`trust-lime-env` skill) or use `.opencode/scripts/with-trust-env.sh`.

## Reference (upstream)

- https://cea-trust-platform.readthedocs.io/en/v1.9.8/TRUST_Validation.html
- https://cea-trust-platform.readthedocs.io/en/v1.9.8/UserGuide.html
- Datafile-from-scratch and Validation-form tutorials:
  https://cea-trust-platform.readthedocs.io/en/v1.9.8/UserTutorials.html
