@page TrustLiMe_testcases Test cases

The module has **two** kinds of tests:

1. **Unit tests** (`tests/UnitTests/`, GoogleTest) — fast, no mesh, no solver.
   They pin the business logic: data-file keywords, the gradient index
   convention, the problem/equation contracts. Run them first.
2. **Validation cases** (notebooks under
   `share/Validation/Rapports_automatiques/`) — end-to-end runs with analytic
   or invariant references. See @ref TrustLiMe_validation "Validation cases".

---

## Unit tests

```bash
.opencode/scripts/unit-tests.sh --arith-only   # needs only micro + numeric kernels
.opencode/scripts/unit-tests.sh                # needs the full TRUST build
```

| File | Covers |
|---|---|
| `unit_magnetic_field.cpp` | equation keyword, `domaine_application() == "Thermique"` (the BC-compatibility contract), `Pb_MHD` equation count, out-of-range index |
| `unit_induction_source.cpp` | `induction_stretching` keyword; `readOn()` consumes its own braces (regression); unknown keyword is rejected |
| `unit_induction_kernel.cpp` | the arithmetic of \f$S = (b\cdot\nabla)u\f$: index layout, single-component case, accumulation, element-wise application |

`unit_induction_kernel.cpp` deserves a special mention: the kernel velocity
gradient stores component \f$i\cdot\mathrm{dim}+j\f$ for
\f$\partial u_i/\partial x_j\f$, and transposing it still gives plausible
numbers for a diagonal gradient. The test `UsesTheRowMajorKernelGradientLayout`
catches exactly that. The logic is factored into
`src/Induction_stretching_kernel.h` specifically so it can be tested without a
TRUST run.

See `tests/UnitTests/README.txt` for how the test binary is built and linked.

---

## Validation cases

Validation notebooks (and their `.data` datasets) used to check the TRUST-LiMe
module. Each folder under `share/Validation/Rapports_automatiques/` holds a
notebook plus a `src/` directory containing the datasets it runs.

---

## How to run them

The notebooks drive `trustutils.run` and therefore need the **TRUST Python
environment**, not the OpenCode MCP venv:

```bash
cd share/Validation/Rapports_automatiques/<case>/
jupyter nbconvert --to notebook --execute --output-dir /tmp/nb <notebook>.ipynb
```

with the environment prepared first:

```bash
source ../../../env_trust_LiMe.sh
source ../../../trust-code/env_for_python.sh
source ~/Development/venv_trust/bin/activate
make module_semi_optim     # from the repository root
```

Inside a validation folder the datasets live in `src/`; `run.addCase` copies each
dataset into `build/<dataset name>/` where the results (`.son` probes, `.dt_ev`,
…) are written. The notebooks read results from `run.BUILD_DIRECTORY`.

The `/validate` command automates this. From the repository root:

```
/validate share/Validation/Rapports_automatiques/MagneticStandalone/MagneticStandalone.ipynb
```

---

## `MagneticStandalone` — standalone magnetic solver

Notebook: `MagneticStandalone.ipynb` · datasets in `src/`

These cases target the magnetic induction equation solved in standalone form
(see @ref TrustLiMe_model "Problems, equations and boundary conditions"). Every
dataset is **self-contained**: the mesh is generated inline with `Mailler`/`Pave`,
so no `.med` file is required and the cases can be run from a clean checkout.

| Dataset | Physics | Reference solution | What it catches |
|---|---|---|---|
| `invariance.data` | uniform \f$b\f$, \f$u = 0\f$ | exact invariant: \f$b(t)\equiv b_0\f$ | spurious sources, wrong time factor, bad CL assembly, wrong default velocity |
| `diffusion.data` | transverse Fourier mode, \f$\eta = 0.05\f$ | \f$b(t) = b_0\,e^{-\eta\pi^2 t}\f$ | wrong magnetic diffusivity scaling, diffusion operator, Dirichlet CL |
| `diffusion_refined.data` | same, finer mesh | same | mesh convergence (the error must drop) |
| `advection.data` | Gaussian bump advected by \f$u = (0.2, 0)\f$ | peak conservation + transport direction | convection operator, `vitesse_magnetique` wiring |
| `vector_components.data` | **vector** unknown (default `nb_composantes`) | two distinct eigenmodes, rates \f$2\eta\pi^2\f$ and \f$5\eta\pi^2\f$ | component independence; catches a component swap or mixed assembly |
| `steady_state.data` | equilibrium \f$b = y\f$ with **non-uniform** \f$\eta(x) = 1+x\f$ | exact steady state | spurious source, mis-assembled diffusion, functional-\f$\eta\f$ reading path |
| `varying_velocity.data` | **spatially varying** velocity \f$u = (-y, x)\f$, uniform \f$b\f$ | exact invariant | `Champ_Don → Champ_Inc` transfer onto faces, wrong velocity components |
| `source_stretching.data` | M1 source \f$S = (b\cdot\nabla)u\f$, \f$u = (x, 0)\f$ | \f$b_0(t) = B_0 e^{t}\f$, \f$b_1\equiv 0\f$ | source magnitude, sign, gradient-index order, per-component structure |
| `error_missing_eta.data` | no `diffusivite_magnetique` | must fail cleanly | required-keyword diagnostics |
| `error_bad_nbcomposantes.data` | `nb_composantes 7` in 2D | must fail cleanly | range validation |
| `error_bad_bc.data` | unknown CL keyword | must fail cleanly | keyword error reporting |

Design choices worth knowing:

- **`nb_composantes 1`** in most cases — the natural reduction of
  \f$\nabla\cdot b = 0\f$ for a purely transverse field keeps the datasets
  simple. `vector_components.data` deliberately omits the keyword to exercise
  the **default** (one component per space direction).
- **Different eigen-rates per component.** In `vector_components.data` the two
  components are given modes with *different* decay rates
  (\f$\sin(\pi x)\sin(\pi y)\f$ and \f$\sin(2\pi x)\sin(\pi y)\f$), so a
  component swap or a mixed assembly is caught rather than hidden behind
  identical rates.
- **Probes use `grav`** — they are snapped to the nearest cell centre. The
  diffusion check therefore compares the *amplitude ratio* \f$b(t)/b(0)\f$,
  which is independent of where the probe sits on the mesh.
- **The invariance cases are the strictest checks** — they must hold to machine
  precision, so any regression in the operator wiring shows up immediately,
  regardless of mesh.
- **Non-uniform \f$\eta\f$ and velocity** are given as `Champ_Fonc_xyz`
  functional fields, which is the realistic MHD situation (a Hartmann layer has
  a position-dependent \f$\eta_{\rm eff}\f$) and exercises a different reading
  path than a uniform `Champ_Uniforme`.
- **Negative cases are run directly** with the `trust` command rather than
  through `runCases()` (which would abort the notebook); the notebook asserts a
  non-zero exit code and a non-empty diagnostic.

---

## Historical cases

`HartmannFlow` and `Shercliff_3D_duct` are placeholders for the full coupled
MHD cases. They still reference the pre-rewrite dataset layout (they run a
Navier–Stokes + energy + magnetic system), which is **not** what the current
standalone module solves. They are kept for reference and will be reworked once
the Lorentz force / M1 source term are implemented.
