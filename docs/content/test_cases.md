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
| `unit_magnetic_field.cpp` | equation keyword, `domaine_application() == "Thermique"` (the BC-compatibility contract), the `Pb_Induction` / `Pb_MHD` problem hierarchy (equation counts, momentum-before-magnetic order), out-of-range index |
| `unit_induction_source.cpp` | `induction_stretching` keyword; `readOn()` consumes its own braces (regression); unknown keyword is rejected |
| `unit_induction_kernel.cpp` | the arithmetic of \f$S = (b\cdot\nabla)u\f$: index layout, single-component case, accumulation, element-wise application |
| `unit_mhd_postraitement_kernel.cpp` | the arithmetic of the post-treatment fields: \f$|b|^2\f$, energy, cyclotron, \f$u\cdot b\f$, Poynting, curl/helicity index order, \f$Rm\f$, \f$Pm\f$ |
| `unit_lorentz_source.cpp` | the `force_lorentz` data-file contract: keyword registration, `readOn()` consumes its own braces, the `mu0` override and its fallback to the bound magnetic equation; rejects a non-positive `mu0`, an unknown keyword, and a problem with no magnetic equation |

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

A notebook covers **one validation form** — it may run several cases of
that form (its mesh-convergence scales `coarse`/`standard`/`fine`,
substituted from the single template `src/<case>.data`) — and contains
tests of **one single category**. Following the TRUST validation-form layout
(`Rapports_automatiques/<Category>/<FormName>/<FormName>.ipynb`), the standalone
magnetic folder ships one *form folder* per dataset, each containing
`<FormName>.ipynb` + `src/<case>.data` (`MagneticStandalone/Invariance/`,
`MagneticStandalone/Diffusion/`, …, `MagneticStandalone/ErrorBadBc/`).
Each form exists **in 2D and in 3D**: `MagneticStandalone3D/<FormName>/` is the
3D twin of `MagneticStandalone/<FormName>/` (same physics, same assertions;
the dataset gains `dimension 3`, three-component vectors, and `Polyedriser dom`
after `Mailler` — PolyMAC_MPFA needs polyhedral cells in 3D). The
`.data` files are **computations**, not tests — a dataset such as `diffusion`
feeds its validation form (`Diffusion/Diffusion.ipynb`), which carries the
mesh-convergence scan itself (coarse = baseline, standard = ×2, fine = ×4,
all instantiated from the single template `src/diffusion.data`).

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
holds `Invariance/`, `Diffusion/`, `Advection/`,
`SteadyState/`, `VaryingVelocity/`, `SourceStretching/`, `Hartmann1D/`,
`Shercliff2D/`, `MMS_2D/` (the four historical MMS 2D cases fused: `MMS_Diff2D`, `MMS_Trig2D`,
`MMS_ExpTrig2D`, `MMS_Poly_2D`) and the merged negative
`ErrorInputs/` form (the historical `error_missing_eta`,
`error_bad_nbcomposantes`, `error_bad_bc` datasets);
each form folder contains its notebook + `src/<case>.data` (the single
trustutils template with `$nx/$ny` placeholders when the form carries a
mesh-convergence scan). These cases
target the magnetic induction equation solved in standalone form.

### MagneticCoupled (low-Rm with Lorentz force)

The `force_lorentz` keyword activates a one-way coupled momentum equation
via `J × B`. The **two-way** mode adds the `vitesse_hydraulique` keyword on
the magnetic equation (solved NS velocity as transport velocity, bound in
`completer()`). Five forms cover the coupled path (`MMS_Coupled` carries both
geometries — its 2D case and its 3D twin `mms_coupled_3d` in the same form):

| Form | What it validates |
|---|---|
| `ForceLorentzBalance/` | hydrostatic balance with \f$b=(y,0)\f$: \f$\Delta p = -0.18 \pm 10\%\f$, \f$u \to 0\f$ |
| `MMS_Coupled/` | MMS with \f$b=(y,-x)\f$ — **both force components**: \f$\Delta p = -0.60 \pm 5\%\f$ (its scan's standard level *is* the absorbed `MMS_CoupledRefined` twin); the same form also carries the **3D twin** as a second dataset (`mms_coupled_3d`, coarse-mesh regression) |
| `Coupled_Feedback/` | both coupling modes: the baseline documents the one-way limit (\f$b\f$ frozen to machine precision with \f$u_0 \neq 0\f$), the merged `twoway_feedback.data` case (`vitesse_hydraulique`) asserts the two-way discriminator \f$10^{-3} < \max_t \|\mathbf{b}-\mathbf{b}_0\| < 0.5\f$ |
| `HartmannTwoWay/` | the **quantitative** two-way follow-up announced by `Coupled_Feedback`: a channel driven by an imposed pressure difference with both feedbacks live — the solved \f$u\f$ relaxes to the analytic Hartmann profile (\f$\mathrm{Ha}=5\f$, centreline 12.7× below Poiseuille, core error < 1 %), the stretching source induces the analytic \f$b_x\f$ from the *solved* velocity, and the open boundaries impose the analytic magnetic-pressure profile \f$p(y) = P - b_x(y)^2/(2\mu_0)\f$ (which kills the spurious cross-flow a uniform \f$p\f$ would leave); a **Hartmann-number scan** (\f$\mathrm{Ha} \in \{2.5, 5, 7.9\}\f$) maps the suppression factor \f$\sigma(\mathrm{Ha}) = \mathrm{Ha}^2/(2(1-1/\cosh\mathrm{Ha}))\f$ — the low-Rm regime map of PLAN_mhd_coupled §2.5 |
| `ErrorInputs/` | the four merged malformed-input datasets (`error_bad_force`, `error_missing_ps`, `error_two_way_no_ns`, `error_two_way_both_velocities`): each rejected with a non-zero exit and a readable diagnostic (also removing the historical duplicate `Error_MissingPressureSolver` row) |

The **Ha axis** of the low-Rm regime map is covered by `HartmannTwoWay`'s
Hartmann-number scan (above); the **Re axis** and the **Shercliff 3D duct**
(Tassone et al. Fig. 6) remain open targets. The coupled mode itself is
documented in @ref TrustLiMe_model "Problems, equations and boundary conditions".

Every dataset is **self-contained**: the mesh is generated inline with `Mailler`/`Pave`,
so no `.med` file is required and the cases can be run from a clean checkout.

| Dataset | Physics | ASN28 category | Reference solution | What it catches |
|---|---|---|---|---|
| `invariance.data` | uniform \f$b\f$, \f$u = 0\f$ | Verification | exact invariant: \f$b(t)\equiv b_0\f$ | spurious sources, wrong time factor, bad CL assembly, wrong default velocity |
| `diffusion.data` | transverse Fourier mode, \f$\eta = 0.05\f$ | Validation – separate effects | \f$b(t) = b_0\,e^{-\eta\pi^2 t}\f$ | wrong magnetic diffusivity scaling, diffusion operator, Dirichlet CL; its **mesh-convergence scan** (coarse/standard/fine) now carries the refined-mesh check that the removed `DiffusionRefined` form used to provide |
| `advection.data` | Gaussian bump advected by \f$u = (0.2, 0)\f$ | Validation – separate effects | peak conservation + transport direction | convection operator, `vitesse_magnetique` wiring |
| `steady_state.data` | equilibrium \f$b = y\f$ with **non-uniform** \f$\eta(x) = 1+x\f$ | Validation – separate effects | exact steady state | spurious source, mis-assembled diffusion, functional-\f$\eta\f$ reading path |
| `varying_velocity.data` | **spatially varying** velocity \f$u = (-y, x)\f$, uniform \f$b\f$ | Verification | exact invariant | `Champ_Don → Champ_Inc` transfer onto faces, wrong velocity components |
| `source_stretching.data` | M1 source \f$S = (b\cdot\nabla)u\f$, \f$u = (x, -y)\f$ (div-free) | Validation – separate effects | \f$b(t) = B_0 e^{t}\f$ | source magnitude, sign, gradient-index order |
| `hartmann_1d.data` | induced field of the Hartmann profile (\f$\mathrm{Ha}=5\f$), insulating walls, \f$b=(b_x,B_0)\f$ (`nb_composantes 2`) | Validation – separate effects | mean-free \f$b_x(y) = -\tfrac{B_0}{\eta}\big[\int_{-h}^{y}u - \tfrac12\int_{-h}^{h}u\big]\f$ | multi-component transport, M1 source driving \f$b_x\f$ through the applied \f$b_y\f$, insulating walls, odd symmetry, \f$b_y\equiv B_0\f$ |
| `shercliff_2d.data` | induced field of the parabolic no-slip channel profile, insulating walls, \f$b=(b_x,B_0)\f$ (`nb_composantes 2`) | Validation – separate effects | \f$b_x(y) = \tfrac{B_0 U_0}{\eta}\big(\tfrac{y^3}{3h^2}-y\big)\f$ and \f$J_z(y) = \tfrac{B_0 U_0}{\eta\mu_0}\big(1-(y/h)^2\big)\f$ | M1 source with a no-slip shear profile, `courant_magnetique` post-treatment (interior parabola, zero at walls), \f$b_y\equiv B_0\f$ |
| `shercliff_3d.data` | 3D twin (`MagneticStandalone3D/Shercliff3D/`): same 1D physics on a polyhedral mesh (`Polyedriser dom`), full vector \f$b=(b_x,B_0,0)\f$ (`nb_composantes 3`) | Validation – separate effects | same analytic profile, plus the exact extrusion invariants \f$b_z\equiv 0\f$, z-uniformity, and the vector \f$\mathbf{J}=(0,0,J_z(y))\f$ | full vector model in 3D, 3-component `courant_magnetique`, `Polyedriser` mesh path, probe snapping in 3D |
| `error_missing_eta.data` | no `diffusivite_magnetique` | Verification | must fail cleanly | required-keyword diagnostics |
| `error_bad_nbcomposantes.data` | `nb_composantes 7` in 2D (also invalid in 3D) | Verification | must fail cleanly | range validation |
| `error_bad_bc.data` | unknown CL keyword | Verification | must fail cleanly | keyword error reporting |

The category column tags the **test** (assertion set), not the dataset: e.g.
the decay test lives in `Diffusion/Diffusion.ipynb` alone, whose
mesh-convergence scan covers what the historical `DiffusionRefined` twin
did at a separate level (L1 = its mesh, now one of the scan levels).

Design choices worth knowing:

- **Every standalone form has a 3D twin** under `MagneticStandalone3D/<FormName>/`
  (the R5 gap on `Shercliff` is closed: `Shercliff3D/` twins `Shercliff2D/`).
  The 3D suite differs in one way: `MMS_3D/` holds the four historical 3D MMS
  cases fused (2D counterpart `MMS_2D/`). Twin
  forms keep the same physics and assertions; the datasets differ only in
  `dimension 3`, three-component vectors/probes and `Polyedriser dom` after
  `Mailler`. Meshes keep the 2D resolution only in the direction that matters
  (e.g. the diffusion mode direction) and stay coarse elsewhere, so the 3D runs
  are affordable. `Shercliff3D` additionally asserts the exact invariants of
  the extrusion (`b_z = 0`, z-uniformity, vector `courant_magnetique`) and is
  the only 3D standalone form running the full vector model (`nb_composantes 3`).
- **`nb_composantes`** — most standalone cases transport a single component
  (`nb_composantes 1`), the natural reduction of \f$\nabla\cdot b = 0\f$ for a
  purely transverse field. The multi-component (vector) unknown is exercised by
  `hartmann_1d` and `shercliff_2d` (`nb_composantes 2`, so the M1 source can
  drive the induced \f$b_x\f$ through the applied \f$b_y\f$), by `shercliff_3d`
  (`nb_composantes 3`, required by the 3D `courant_magnetique` post-treatment)
  and by the `Postraitement_fields` forms; the valid range is
  \f$[1, \mathrm{dimension}]\f$ (`error_bad_nbcomposantes`
  pins the rejection of out-of-range values).
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
- **`Hartmann1D` pins a known limitation.** The M1 source needs the cell gradient
  of the frozen velocity; the kernel assembles it by *averaging face gradients*
  (`grad_Champ_Face_PolyMAC_MPFA::update_ge()`), and a boundary face only
  receives a Dirichlet contribution. The frozen velocity carries no hydrodynamic
  wall condition here, so the two cells adjacent to a wall get a biased
  \f$\nabla u\f$ (for a linear velocity its value is exactly halved there —
  isolated by running the source alone with \f$\eta=0\f$). The induced field is
  therefore compared **tightly in the duct core** and only against a
  **documented bound** inside the Hartmann layer; the deviation is odd, converges
  at first order, and that bound is a regression guard to be tightened once the
  boundary treatment is fixed (see `KERNEL_REUSE_NOTES.md`).
- **Negative cases are run directly** with the `trust` command rather than
  through `runCases()` (which would abort the notebook); each negative form
  (`Error*`) asserts a non-zero exit code and a non-empty diagnostic.

---

## `Postraitement_fields` — derived MHD post-treatment fields

Two forms, merged by dimension, covering every post-treatment field of
@ref TrustLiMe_model "Problems, equations and boundary conditions" §4bis. The
fields split into two physical families that cannot share one state, so each
form carries **two datasets** (the `MMS_2D` pattern: several `.data` templates,
one notebook, one form folder):

| Form / dataset | Fields | Configuration |
|---|---|---|
| `Champs2D` / `champs2d_gradient` | `divergence_magnetique`, `rotationnel_magnetique`, `courant_magnetique`, `force_lorentz`, `tension_magnetique` | 2D, \f$ b = (x-y,\,x+y) \f$ exactly linear, \f$ \eta = 0 \f$, no velocity, \f$ \mu_0 = 2 \f$ |
| `Champs2D` / `champs2d_transport` | `vecteur_poynting`, `helicite_croisee`, `nombre_de_reynolds_magnetique`, `nombre_de_prandtl_magnetique`, `energie_magnetique`, `pulsation_cyclotronique` | 2D, \f$ b = (3,4) \f$, \f$ u = (4,3) \f$ uniform, \f$ \eta = 0.25 \f$, \f$ \mu = 2 \f$, \f$ \rho = 4 \f$, \f$ \mu_0 = 2 \f$, \f$ q/m = 2 \f$ |
| `Champs3D` / `champs3d_gradient` | `courant_magnetique` (full vector) | 3D, \f$ b = (y,z,x) \f$ exactly linear, \f$ \eta = 0 \f$, \f$ \mu_0 = 2 \f$ |
| `Champs3D` / `champs3d_transport` | `helicite_cinetique` + the scalar-model (`nb_composantes 1`) branch of `energie_magnetique` and `pulsation_cyclotronique` | 3D, \f$ u = (y,z,x) \f$ exactly linear, \f$ b = 1 \f$, \f$ \eta = 1 \f$, \f$ \mu_0 = 2 \f$, \f$ q/m = 2 \f$ |

| Field | Checked value | Probe |
|---|---|---|
| `divergence_magnetique` | \f$ \nabla\cdot b = 2 \f$ | interior \f$ (0.45,0.55) \f$ **and** wall cell \f$ (0.05,0.55) \f$ |
| `rotationnel_magnetique` | \f$ w_z = 2 \f$ | \f$ (0.45,0.55) \f$ |
| `courant_magnetique` | \f$ J_z = 1 \f$; 3D \f$ J = (-0.5,-0.5,-0.5) \f$ | \f$ (0.45,0.55) \f$; \f$ (0.375)^3 \f$ |
| `force_lorentz` | \f$ F = (-1,\,-0.1) \f$ | \f$ (0.45,0.55) \f$ |
| `tension_magnetique` | \f$ F_t = (-0.55,\,0.45) \f$ (= \f$ (-y,x) \f$) | \f$ (0.45,0.55) \f$ |
| `vecteur_poynting` | \f$ S = (14,\,-10.5) \f$ | \f$ (0.45,0.55) \f$ |
| `helicite_croisee` | \f$ u\cdot b = 24 \f$ | \f$ (0.45,0.55) \f$ |
| `nombre_de_reynolds_magnetique` | \f$ R_m = |u|\,L_e/\eta = 20\,h \f$ | \f$ (0.45,0.55) \f$ |
| `nombre_de_prandtl_magnetique` | \f$ \nu/\eta = 2 \f$ | \f$ (0.45,0.55) \f$ |
| `energie_magnetique` | \f$ |b|^2/(2\mu_0) = 6.25 \f$ (vector), \f$ 0.25 \f$ (scalar) | \f$ (0.45,0.55) \f$; \f$ (0.375)^3 \f$ |
| `pulsation_cyclotronique` | \f$ (q/m)|b| = 10 \f$ (vector), \f$ 2 \f$ (scalar) | \f$ (0.45,0.55) \f$; \f$ (0.375)^3 \f$ |
| `helicite_cinetique` | \f$ u\cdot(\nabla\times u) = -(x+y+z) \f$ | \f$ (0.375)^3 \f$ |

Design notes (details in `KERNEL_REUSE_NOTES.md` §11ter):

- **Every constant is non-default** (\f$ \mu_0 \f$, \f$ q/m \f$, \f$ \mu \f$,
  \f$ \rho \f$, \f$ \eta \f$), so an implementation with a hard-coded \f$ 1 \f$
  cannot pass.
- **The linear field keeps both invariants alive**: \f$ b=(x-y,\,x+y) \f$ gives
  \f$ \nabla\cdot b \neq 0 \f$ *and* \f$ (\nabla\times b)_z \neq 0 \f$.
  \f$ b=(x,y) \f$ would make the curl/current/Lorentz checks vacuous and
  \f$ b=(-y,x) \f$ would make the divergence check vacuous.
- **The two transport vectors are neither parallel nor orthogonal**
  (\f$ |b|=|u|=5 \f$, \f$ u\cdot b = 24 \f$): parallel kills the Poynting
  vector, orthogonal kills the crossed helicity.
- **Every reference is exactly representable in 9 significant digits**, the
  precision a `.son` probe file stores — a value such as \f$ 2\sqrt{5} \f$
  could not be asserted to machine precision.
- Each dataset carries the shared 3-level mesh-convergence ladder
  (`coarse`/`standard`/`fine`, cells ×2 / ×4) and asserts the identity at
  **every** level.

Categories (ASN 28): *Validation – separate effects* — each field checks one
physical convention (formula/sign/normalisation) against a closed-form value.
The forms exercise both the post-processing plumbing
(`creer_champ`/`has_champ`/`get_champ`, probes through `Postraitement`) and the
elementwise arithmetic — which is pinned harder (layout conventions) by
`unit_mhd_posttraitement_kernel.cpp` without a mesh at all.

---

## Historical cases

`HartmannFlow` and `Shercliff_3D_duct` are kept for reference only: they
still reference the pre-rewrite dataset layout (a Navier–Stokes + energy +
magnetic system), which is **not** what the current standalone module solves.
The coupled low-Rm mode (`force_lorentz` on the momentum equation) is now wired
in `Pb_MHD` (laminar) and `Pb_MHD_Turbulent` (k-eps) and validated by
`MagneticCoupled/ForceLorentzBalance/`;
the driven-Hartmann physics the historical `HartmannFlow` case aimed at is
now covered — quantitatively and with both feedbacks live — by
`MagneticCoupled/HartmannTwoWay/`.
The Shercliff 3D duct benchmark remains an open coupled-case target.
