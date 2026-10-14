@page TrustLiMe_validation Validation cases

Validation cases are end-to-end runs driven by Jupyter notebooks, as opposed to
the mesh-less GoogleTest @ref TrustLiMe_testcases "unit tests". They check the
module against analytic solutions or exact invariants of the induction equation.

Each case lives in its own folder under
`share/Validation/Rapports_automatiques/`:

```
share/Validation/Rapports_automatiques/
├── MagneticStandalone/             # standalone induction equation (2D)
│   ├── Invariance/                 # one validation form per test case
│   │   ├── Invariance.ipynb        # notebook named after its form folder
│   │   └── src/invariance.data
│   ├── Diffusion/  DiffusionRefined/  Advection/
│   ├── SteadyState/  VaryingVelocity/  SourceStretching/
│   └── ErrorMissingEta/  ErrorBadNbcomposantes/  ErrorBadBc/
├── MagneticStandalone3D/           # the same 10 forms, one-to-one, in 3D
│   └── <FormName>/…                # (Polyedriser dom after Mailler — see below)
├── HartmannFlow/             # placeholder for the coupled MHD case
│   ├── HartmannFlow.ipynb
│   └── src/
└── Shercliff_3D_duct/        # placeholder for the coupled MHD case
    ├── Shercliff_3D_duct.ipynb
    └── src/
```

The `MagneticStandalone3D` forms differ from their 2D counterpart only in the
dataset: `dimension 3`, three-component vector data, three-coordinate probes,
and **`Polyedriser dom`** right after `Mailler` (PolyMAC_MPFA requires polyhedral
cells in 3D; the `Pave` hexahedra are converted in place). The notebooks and the
asserted tolerances are shared with the 2D forms.

Four formatting rules are enforced by `.opencode/scripts/check-validation.sh`
(`/check-validation`): the notebook lives at the form root only —
`Rapports_automatiques/<Category>/<FormName>/<FormName>.ipynb`, never in
`src/`; datasets are **sequential-only** (no `Decouper`/`Scatter` partition
machinery in a `.data`, no `partition()` / `nbProcs>1` in a notebook); and a
notebook runs **one and only one test case** (one `addCase` per notebook —
see the `trust-lime-notebooks` skill).

---

## Running a case

The notebooks drive the `trustutils` Python package
(`trustutils.run`: `addCase → runCases → tablePerf`), so they must run inside the
**TRUST Python environment** rather than the agent's own virtual environment:

```bash
source ../trust-code/env_TRUST.sh
source ../trust-code/env_for_python.sh
source ~/Development/venv_trust/bin/activate

jupyter nbconvert --to notebook --execute \
  --output-dir /tmp/opencode/trust-lime-notebooks \
  share/Validation/Rapports_automatiques/HartmannFlow/HartmannFlow.ipynb
```

Inside a case folder, the datasets live in `src/`; `run.addCase` copies each
dataset into `build/<dataset name>/` where the results (`.son` probes, `.dt_ev`,
…) are written. The notebooks read results back from `run.BUILD_DIRECTORY`.

### PDF report

Each form can be exported to PDF and the per-form PDFs concatenated into a
single report at the repository root (`MagneticStandalone_validation.pdf`,
20 forms: the 10 2D then the 10 3D), via
`.opencode/scripts/run-validation-pdf.sh` (execute → `--to pdf` →
`pdfunite`). The report is a generated artifact and stays untracked; the rule
lives in the `trust-lime-notebooks` skill ("PDF export & the repository
report").

---

## The cases

### `MagneticStandalone`

Targets the standalone magnetic induction equation currently implemented by the
module. Every dataset is self-contained (the mesh is built inline with
`Mailler`/`Pave`), so no `.med` file is needed. The individual datasets, their
references, and what each one catches are listed in
@ref TrustLiMe_testcases "Test cases → `MagneticStandalone`".

Per ASN Guide n° 28 a notebook holds tests of **one single category**, and it
runs **one and only one test case**. The folder therefore ships one validation
form per dataset (`MagneticStandalone/<FormName>/<FormName>.ipynb`), covering
exact invariants, mesh convergence, malformed-input diagnostics and isolated
phenomena against analytical solutions; a future Verification/Validation
category split folds into the form naming.

### `HartmannFlow` and `Shercliff_3D_duct`

Placeholders for the full coupled MHD cases. They still reference the
pre-rewrite dataset layout (a Navier–Stokes + energy + magnetic system), which is
**not** what the current standalone module solves. They are kept for reference
and will be reworked once the Lorentz force (M2) and the divergence-free
projection (M3) land — see @ref TrustLiMe_model "Problems, equations and boundary
conditions".
