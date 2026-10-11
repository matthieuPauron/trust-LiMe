@page TrustLiMe_validation Validation cases

Validation cases are end-to-end runs driven by Jupyter notebooks, as opposed to
the mesh-less GoogleTest @ref TrustLiMe_testcases "unit tests". They check the
module against analytic solutions or exact invariants of the induction equation.

Each case lives in its own folder under
`share/Validation/Rapports_automatiques/`:

```
share/Validation/Rapports_automatiques/
├── MagneticStandalone/       # standalone induction equation (current module)
│   ├── MagneticStandalone.ipynb
│   └── src/*.data
├── HartmannFlow/             # placeholder for the coupled MHD case
│   ├── HartmannFlow.ipynb
│   └── src/
└── Shercliff_3D_duct/        # placeholder for the coupled MHD case
    ├── Shercliff_3D_duct.ipynb
    └── src/
```

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

---

## The cases

### `MagneticStandalone`

Targets the standalone magnetic induction equation currently implemented by the
module. Every dataset is self-contained (the mesh is built inline with
`Mailler`/`Pave`), so no `.med` file is needed. The individual datasets, their
references, and what each one catches are listed in
@ref TrustLiMe_testcases "Test cases → `MagneticStandalone`".

### `HartmannFlow` and `Shercliff_3D_duct`

Placeholders for the full coupled MHD cases. They still reference the
pre-rewrite dataset layout (a Navier–Stokes + energy + magnetic system), which is
**not** what the current standalone module solves. They are kept for reference
and will be reworked once the Lorentz force (M2) and the divergence-free
projection (M3) land — see @ref TrustLiMe_model "Problems, equations and boundary
conditions".
