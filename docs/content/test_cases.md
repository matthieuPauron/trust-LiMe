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
| `unit_mhd_postraitement_kernel.cpp` | the arithmetic of the post-treatment fields: \f$|b|^2\f$, energy, cyclotron, \f$u\cdot b\f$, Poynting, curl/helicity index order, \f$Rm\f$, \f$Pm\f$ |

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

### Test categories (ASN Guide n° 28)

Each validation **test** — a notebook case run *plus* its assertions — carries
one category, following the official English translation of **ASN Guide n° 28**
(*Qualification of scientific computing tools used in the nuclear safety case*,
25/07/2017):

| Category | Guide ref. | Meaning |
|---|---|---|
| **Verification** | § 3.2 | the equations are solved correctly, numerically and data-processing wise: exact invariants, numerical convergence, error handling of malformed input |
| **Validation – separate effects** | § 3.3 | one isolated physical phenomenon reproduced against an *analytical solution* |
| **Validation – integral** | § 3.3 | all phenomena and their interactions (reserved: none yet in the standalone phase) |

A notebook runs **one and only one test case** and contains tests of **one
single category**. Following the TRUST validation-form layout
(`Rapports_automatiques/<Category>/<FormName>/<FormName>.ipynb`), the standalone
magnetic folder ships one *form folder* per dataset, each containing
`<FormName>.ipynb` + `src/<case>.data` (`MagneticStandalone/Invariance/`,
`MagneticStandalone/Diffusion/`, …, `MagneticStandalone/ErrorBadBc/`).
Each form exists **in 2D and in 3D**: `MagneticStandalone3D/<FormName>/` is the
3D twin of `MagneticStandalone/<FormName>/` (same physics, same assertions;
the dataset gains `dimension 3`, three-component vectors, and `Polyedriser dom`
after `Mailler` — PolyMAC_MPFA needs polyhedral cells in 3D). The
`.data` files are **computations**, not tests — a dataset such as `diffusion`
feeds both a Validation test (decay against the analytic solution, in
`Diffusion/Diffusion.ipynb`) and, together with `diffusion_refined`, a
Verification test (mesh convergence, whose per-case bounds are asserted in
`Diffusion/Diffusion.ipynb` and `DiffusionRefined/DiffusionRefined.ipynb`).

New tests are written in a **fixed sequence** — a test of a later stage may
only be added once all earlier stages are green for that case:

1. **Verification** first — the equations are solved correctly, numerically
   and data-processing wise (exact invariants, mesh convergence,
   malformed-input diagnostics);
2. then **validation against analytical data** — isolated phenomena
   reproduced against closed-form solutions;
3. and only finally **comparison to experiments** — full problem interaction
   against measured data (e.g. a Hartmann flow profile), i.e. the
   *Validation – integral* category; none exists yet in the standalone phase.

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
/check-validation   # R1-R4 formatting gate — must be green first
/validate share/Validation/Rapports_automatiques/MagneticStandalone/Diffusion/Diffusion.ipynb
```

---

## `MagneticStandalone` — standalone magnetic solver

One validation form per test case (TRUST layout
`Rapports_automatiques/<Category>/<FormName>/<FormName>.ipynb`): the folder
holds `Invariance/`, `Diffusion/`, `DiffusionRefined/`, `Advection/`,
`SteadyState/`, `VaryingVelocity/`, `SourceStretching/`
and the negative `Error_missing_eta` – `Error_bad_nbcomposantes` –
`Error_bad_bc` forms; each form folder contains its notebook + `src/<case>.data`

These cases target the magnetic induction equation solved in standalone form
(see @ref TrustLiMe_model "Problems, equations and boundary conditions"). Every
dataset is **self-contained**: the mesh is generated inline with `Mailler`/`Pave`,
so no `.med` file is required and the cases can be run from a clean checkout.

| Dataset | Physics | ASN28 category | Reference solution | What it catches |
|---|---|---|---|---|
| `invariance.data` | uniform \f$b\f$, \f$u = 0\f$ | Verification | exact invariant: \f$b(t)\equiv b_0\f$ | spurious sources, wrong time factor, bad CL assembly, wrong default velocity |
| `diffusion.data` | transverse Fourier mode, \f$\eta = 0.05\f$ | Validation – separate effects | \f$b(t) = b_0\,e^{-\eta\pi^2 t}\f$ | wrong magnetic diffusivity scaling, diffusion operator, Dirichlet CL |
| `diffusion_refined.data` | same, finer mesh | Verification (mesh convergence) | same | mesh convergence (the error must drop) |
| `advection.data` | Gaussian bump advected by \f$u = (0.2, 0)\f$ | Validation – separate effects | peak conservation + transport direction | convection operator, `vitesse_magnetique` wiring |
| `steady_state.data` | equilibrium \f$b = y\f$ with **non-uniform** \f$\eta(x) = 1+x\f$ | Validation – separate effects | exact steady state | spurious source, mis-assembled diffusion, functional-\f$\eta\f$ reading path |
| `varying_velocity.data` | **spatially varying** velocity \f$u = (-y, x)\f$, uniform \f$b\f$ | Verification | exact invariant | `Champ_Don → Champ_Inc` transfer onto faces, wrong velocity components |
| `source_stretching.data` | M1 source \f$S = (b\cdot\nabla)u\f$, \f$u = (x, -y)\f$ (div-free) | Validation – separate effects | \f$b(t) = B_0 e^{t}\f$ | source magnitude, sign, gradient-index order |
| `error_missing_eta.data` | no `diffusivite_magnetique` | Verification | must fail cleanly | required-keyword diagnostics |
| `error_bad_nbcomposantes.data` | `nb_composantes 7` in 2D (also invalid in 3D) | Verification | must fail cleanly | range validation |
| `error_bad_bc.data` | unknown CL keyword | Verification | must fail cleanly | keyword error reporting |

The category column tags the **test** (assertion set), not the dataset: e.g.
the mesh-convergence test spans the `DiffusionRefined` form (refined bound)
and the `Diffusion` form (coarse bound), while the decay test lives in
`Diffusion/Diffusion.ipynb` alone.

Design choices worth knowing:

- **Every case has a 3D twin** under `MagneticStandalone3D/<FormName>/`. The 3D
  forms keep the same physics and assertions; the datasets differ only in
  `dimension 3`, three-component vectors/probes and `Polyedriser dom` after
  `Mailler`. Meshes keep the 2D resolution only in the direction that matters
  (e.g. the diffusion mode direction) and stay coarse elsewhere, so the 3D runs
  are affordable.
- **`nb_composantes 1`** in all cases — the natural reduction of
  \f$\nabla\cdot b = 0\f$ for a purely transverse field keeps the datasets
  simple. The multi-component (vector) unknown is a later phase (the
  `discretiser()` currently rejects `nb_composantes != 1`); a former
  `VectorComponents` form was removed when it became clear the scalar-only
  module cannot run it.
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
  through `runCases()` (which would abort the notebook); each negative form
  (`Error*`) asserts a non-zero exit code and a non-empty diagnostic.

---

## `Postraitement_fields` — derived MHD post-treatment fields

One form per post-treatment field of
@ref TrustLiMe_model "Problems, equations and boundary conditions" §4bis, each
with one 1-probe dataset asserting the analytic value of the field on a
uniform (or exactly-linear) configuration:

| Form | Field | Configuration | Checked value |
|---|---|---|---|
| `EnergieMagnetique` | `energie_magnetique` | $b_0 = 2.5$, $\mu_0 = 2$ | $6.25/4 = 1.5625$ |
| `PulsationCyclotronique` | `pulsation_cyclotronique` | $b_0 = 2.5$, $q/m = 2$ (keyword) | $5$ |
| `VecteurPoynting` | `vecteur_poynting` | $b = (1,2)$, $u = (3,4)$, $\mu_0 = 1$ | $S = (4, -2)$ |
| `HeliciteCroisee` | `helicite_croisee` | $b = (1,2)$, $u = (3,4)$ | $u\cdot b = 11$ |
| `HeliciteCinetique` | `helicite_cinetique` | 3D, $u = (y, z, x)$ (exactly linear) | $-(x{+}y{+}z)$ at $(0.375)^3$ |
| `NombreDeReynoldsMagnetique` | `nombre_de_reynolds_magnetique` | 10×10 mesh, $u = (3,0)$, $\eta = 2$ | $3\cdot 0.1/2 = 0.15$ |
| `NombreDePrandtlMagnetique` | `nombre_de_prandtl_magnetique` | $\mu = 2$, $\rho = 4$, $\eta = 0.25$ | $\nu/\eta = 2$ |

Categories (ASN 28): *Validation – separate effects* — each form checks one
physical convention (formula/sign/normalisation) against a closed-form value.
The forms exercise both the post-processing plumbing
(`creer_champ`/`has_champ`/`get_champ`, probes through `Postraitement`) and the
elementwise arithmetic — which is pinned harder (layout conventions) by
`unit_mhd_posttraitement_kernel.cpp` without a mesh at all.

---

## Historical cases

`HartmannFlow` and `Shercliff_3D_duct` are placeholders for the full coupled
MHD cases. They still reference the pre-rewrite dataset layout (they run a
Navier–Stokes + energy + magnetic system), which is **not** what the current
standalone module solves. They are kept for reference and will be reworked once
the Lorentz force / M1 source term are implemented.
