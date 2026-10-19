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
│   ├── Diffusion/  Advection/
│   ├── SteadyState/  VaryingVelocity/  SourceStretching/
│   ├── Hartmann1D/                 # Hartmann induced field (1D physics)
│   ├── MMS_2D/                     # the four historical MMS cases fused
│   └── ErrorMissingEta/  ErrorBadNbcomposantes/  ErrorBadBc/
├── MagneticStandalone3D/           # the same 11 forms, one-to-one, in 3D
│   └── <FormName>/…                # (Polyedriser dom after Mailler — see below)└── Postraitement_fields/           # one form per post-treatment field
├── MagneticStandalone3D/           # the same forms, one-to-one, in 3D
│   └── <FormName>/…                # (Polyedriser dom after Mailler — see below)
├── MagneticCoupled/                # coupled low-Rm MHD (2D, incl. the merged
│   └── <FormName>/…                #   one-way + two-way feedback form)
├── MagneticCoupled3D/              # the 3D coupled twin
└── Postraitement_fields/           # one form per post-treatment field
    └── <FormName>/…                # (∇b / |b|² / u·b based — see the table below)
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
machinery in a `.data`, no `partition()` / `nbProcs>1` in a notebook); the R4 rule
is **one form per notebook** (see the `trust-lime-notebooks` skill) — a form may run
the baseline dataset plus its mesh-convergence scan levels from
`src/<case>.tpl.data`, and the `MMS_2D`/`MMS_3D` forms run four datasets each.
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
  share/Validation/Rapports_automatiques/MagneticStandalone/Hartmann1D/Hartmann1D.ipynb
```

Inside a case folder, the datasets live in `src/`; `run.addCase` copies each
dataset into `build/<dataset name>/` where the results (`.son` probes, `.dt_ev`,
…) are written. The notebooks read results back from `run.BUILD_DIRECTORY`.

### PDF report

Each form can be exported to PDF and the per-form PDFs concatenated into a
single report at the repository root (`trust-lime_validation.pdf`,
43 forms: the 2D and 3D standalone families (the 3D MMS cases fused as
`MMS_3D`), the coupled-MHD forms and the post-treatment fields — every
positive form now also carrying its mesh-convergence scan), via
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
covers **one validation form** (see the `trust-lime-notebooks` skill; R4):
several datasets of that form are allowed — the baseline plus its
mesh-convergence scan levels (`src/<case>.tpl.data`), and the merged MMS
forms (`MMS_2D`, `MMS_3D`) run four datasets each. The category ships one
folder per form (`<Category>/<FormName>/<FormName>.ipynb`), covering exact
invariants, mesh convergence, malformed-input diagnostics and isolated
phenomena against analytical solutions; a future Verification/Validation
category split folds into the form naming.

### `HartmannFlow` and `Shercliff_3D_duct`

The standalone induction equation does not solve a Navier–Stokes + energy +
magnetic system, so the old placeholders no longer apply. The coupled low-Rm
mode (`force_lorentz` + optional `Navier_Stokes_standard`) is documented in
@ref TrustLiMe_model "Problems, equations and boundary conditions" §4bis and
validated by `MagneticCoupled/ForceLorentzBalance/`. The Shercliff 3D duct
benchmark remains an open coupled-case target (Tassone et al.).

The **standalone half** of the Hartmann problem — the induced field produced by a
given Hartmann velocity profile with insulating walls — is covered today by the
`Hartmann1D` form (`MagneticStandalone/Hartmann1D/` and its 3D twin), see
@ref TrustLiMe_testcases "Test cases".
