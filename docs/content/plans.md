@page TrustLiMe_plans Roadmap and Next Steps

This page condenses the project's planning documents — the `PLAN_*.md` and
`END_GOAL.md` files at the repository root — into one view of what is **done**,
what comes **next**, and what is deliberately **deferred**. The plain-Markdown
originals remain the working documents: they are updated as work proceeds, and
this page follows them.

@note Status snapshot: 24 October 2026. The established baseline is 32
reportable validation forms and 45 unit tests (29 arith-only pass). The opt-in Gauss-law
projection and recent VEF/VKS coupling changes are newer than that baseline;
their targeted checks pass where noted below, but a full baseline regression is
still outstanding. See @ref TrustLiMe_validation "Validation cases" and
@ref TrustLiMe_testcases "Test cases".

---

## 1. Where the project stands

| Milestone | Status |
|---|---|
| M0 — standalone magnetic advection–diffusion | **done** |
| M1 — \f$S = (b\cdot\nabla)u\f$ stretching source (`induction_stretching`) | **done** |
| Coupled low-Rm mode — `force_lorentz` on a hydraulic problem | **done** (one-way \f$b \to\f$ momentum; PolyMAC and VEF source assembly) |
| Post-treatment fields (`Champ_Fonc_MHD`) and their forms (merged 2026-10-21 into `Champs2D` / `Champs3D`) | **done** |
| Coupled validation forms (7) | **done** |
| Full validation suite — 2D, 3D twins, coupled, post-treatment | **32 reportable forms**; latest full PDF sweep ran 31/32 (2026-10-24). `HartmannTwoWay` is currently in progress and failed at `run.reset()` on a non-empty generated scan directory; the other forms, including both Gauss-law dimensions and the flux-error case, passed. |
| Unit tests (GoogleTest) | **29 arith-only tests pass**; the full 45-test run currently exits by SIGSEGV at the first kernel-linked registration test |
| Two-way coupling — NS velocity feeds the magnetic convection (`vitesse_hydraulique`) | **done** (lagged staggered scheme; commits `9e5b13b`, `d484fbd`) |
| \f$\nabla\cdot b = 0\f$ enforcement | **implemented, opt-in** via `correction_divergence`; 2D MPI and 3D serial mesh scans plus incompatible-flux rejection pass |
| Turbulent magnetic diffusivity | **implemented**: prescribed fields, hydraulic k-\f$\varepsilon\f$, and an independent magnetic k-\f$\varepsilon\f$ closure (source-level; smoke case still aborts at `Solve`) |
| Strong two-way coupling | **implemented, opt-in** Picard iteration via `couplage_fort`; default staggered mode unchanged |
| High-\f$\mathrm{Rm}\f$ dynamo | **open / long term** — section 5 |

The standalone induction and low-Rm coupling foundations are complete. Current
near-term work is closing validation and benchmark gaps (especially the Gauss
projection sanitizer/full-unit-test follow-up and 3D coupled duct case); new turbulence equations,
Ohmic heating and high-\f$\mathrm{Rm}\f$ dynamo capability remain larger extensions.

---

## 2. Roadmap at a glance

| Stage | Content | Status | Plan document |
|---|---|---|---|
| Standalone M0/M1 | induction equation + stretching source | done | `PLAN_magnetic_standalone.md` |
| Coupled C0 | kernel plumbing: optional equations, Lorentz source | done | `PLAN_mhd_coupled.md` |
| Coupled C1 | low-Rm momentum coupling | **done** — one-way `force_lorentz`, two-way `vitesse_hydraulique` | `PLAN_mhd_coupled.md` |
| Coupled C2 | low-\f$\mathrm{Rm}\f$ two-way NS+MHD coupling | **done**; close the Re scan and Shercliff 3D benchmark | `PLAN_mhd_coupled.md` |
| Gauss-law projection | opt-in cell-centred Hodge projection | **implemented and validated** in 2D/3D; full-suite and sanitizer follow-up documented | `PLAN_Gauss_law.md` |
| Turbulent induction | effective \f$\eta_t\f$ from fields or hydraulic k-\f$\varepsilon\f$ | **implemented**; magnetic fluctuation equations remain scaffolded | `PLAN_induction_turbulence.md` |
| Coupled advanced physics | magnetic fluctuation transport, Ohmic heating, other benchmarks | open / staged | `PLAN_mhd_coupled.md` |
| Dynamo / VKS | turbulent EDM, high-Rm regime | long term | `END_GOAL.md` |

---

## 3. Near-term work items (standalone polish) {#TrustLiMe_plans_nearterm}

Small, self-contained items that do not change the solver's scope.

### `paroi_conductrice_magnetique` — perfect-conductor wall

The physical condition \f$b \times n = 0\f$ needs per-component conditioning
(Dirichlet on the tangential components, Neumann on the normal one), which the
current one-CL-per-boundary model cannot express. It needs a dedicated CL class
at the PolyMAC face level — deferred pending an API investigation (Phase B.2
of the improved plan). Available substitutes today:
`paroi_isolante_magnetique`, `Neumann_Paroi_adiabatique`, `Paroi`. A
far-field / vacuum boundary condition (non-local by nature) is deferred with
it.

### Magnetic-named boundary conditions

Magnetic boundary conditions reuse the kernel scalar CLs through the
`domaine_application() == "Thermique"` contract (see
@ref TrustLiMe_model "Problems, equations and boundary conditions"). That is
functional but confusing to a data-file author; a full magnetic-vocabulary
sweep is deferred.

### Hartmann wall-gradient bias

The M1 source reads \f$\nabla u\f$ from the kernel's face-gradient average, and
a boundary face only receives a Dirichlet contribution — so the two cells
adjacent to a wall carry a biased gradient (measured exactly halved for a
linear velocity). `Hartmann1D` documents and bounds the effect; the regression
bound should be **tightened once the boundary treatment is fixed**. The fix
itself lives in the kernel
(`grad_Champ_Face_PolyMAC_MPFA::update_ge()`), not in this module.

### 2D/3D twin gaps (R5)

`Shercliff2D` is twinned by `MagneticStandalone3D/Shercliff3D/`, which also
carries the full vector model. `MMS_Coupled` carries its 3D twin as a second
dataset, and `VKSTurbulent` exercises a 3D VEF turbulent case. Still without
3D counterparts are `HartmannTwoWay/`, `ForceLorentzBalance/` and
`Coupled_Feedback/`; the coupled `ShercliffDuct3D` case also remains to be
completed and validated. The `Hartmann1D` 2D/3D forms differ in probe sets by
design. Keep this twin sweep as a coverage goal once the form set stabilises.

### \f$\nabla\cdot b = 0\f$ handling

An opt-in `correction_divergence { }` block now projects the cell-centred
magnetic field onto the null space of the module's reported Green–Gauss
divergence, using the exact discrete operator \f$L = D D^T\f$. The 2D
`GaussLaw` case passes on 2 ranks over coarse/standard/fine meshes, with
post-initialization maximum divergence below \f$2\times10^{-13}\f$.
The 3D `MagneticStandalone3D/GaussLaw` twin passes a sequential
coarse/standard/fine scan on the shared 16/32/64 active-x-cell ladder, with
maximum probe divergence \f$1.43\times10^{-13}\f$. Three cells in y/z avoid a
parity-sensitive null mode in this 3D cell-centred operator.
`ErrorInputs/error_incompatible_flux.data` confirms that nonzero net boundary
flux is rejected with the Gauss-law diagnostic. The full PDF sweep passed
31/32 reportable forms; the remaining failure is the in-progress
`HartmannTwoWay` scan's `run.reset()` on a non-empty generated build directory.
The 45-test kernel-linked unit suite also currently SIGSEGVs before its first
test, while all 29 arith-only tests pass. Sanitizer validation remains open.
The projection supports homogeneous Neumann magnetic walls; non-homogeneous
Dirichlet data are not preserved, and this does not enforce every flux-based
divergence notion used by other discretizations.

The longer-term Nedelec/vector-potential route remains open. It would change the
unknown/discretization and could also enable the missing `helicite_magnetique`
and `helicite_generalisee` fields; it is not required for the implemented
projection's narrower contract.

---

## 4. Coupled MHD — the C-phases {#TrustLiMe_plans_coupled}

From `PLAN_mhd_coupled.md`. Both coupling directions are now implemented:
the Lorentz force \f$J\times B\f$ (explicit, lagged \f$b\f$) acts on the
momentum equation, and — with the `vitesse_hydraulique` keyword — the
magnetic equation transports with the **solved NS velocity**
(`induction_stretching` follows it with a per-step grad(u) rebuild).
The time stepping is a lagged staggered scheme,
\f$b^{s+1}=F(b^s,u^s)\f$, \f$u^{s+1}=G(u^s,b^s)\f$.

### C2 — two-way coupling — **implemented**

- The **NS velocity is the magnetic transport velocity** via the
  `vitesse_hydraulique` pathway (bound in `completer()`, once every
  equation is known). Validated by the two-way discriminator form
  (folded into `Coupled_Feedback` during the notebook-merging pass).
- The `Coupled_Feedback` one-way regression and `ForceLorentzBalance`
  stayed green, plus the two error forms `Error_TwoWayNoNS` /
  `Error_TwoWayBothVelocities`.
- `force_lorentz` now has VEF assembly as well as PolyMAC assembly; the
  `VKSTurbulent` case exercises the VEF turbulent hydraulic path with two-way
  coupling. This expands discretization coverage but is not a high-\f$Rm\f$
  dynamo validation.
- Still open (C2 follow-up): tightened time-step synchronisation if the
  explicit staggered scheme ever shows loose-coupling oscillations.
- Tracked coupled benchmarks:
  - **Ha axis — done.** `HartmannTwoWay`'s Hartmann-number scan
    (\f$\mathrm{Ha} \in \{2.5, 5, 7.9\}\f$) maps the suppression factor
    \f$\sigma(\mathrm{Ha}) = \mathrm{Ha}^2/(2(1-1/\cosh\mathrm{Ha}))\f$
    within 1.5 % at every point.
  - **Re × Ha grid — added in `HartmannTwoWay`.** The 12-point sweep spans
    \f$Re_H \in \{0.01,0.1,1,10\}\f$ and
    \f$Ha \in \{2.5,5,7.9\}\f$ at fixed \f$Rm=0.1\f$. It checks the
    requested Reynolds and magnetic Reynolds numbers, velocity and induced-field
    profiles, suppression factor, cross-flow and applied-field invariant at every point.
    Here \f$Re_H=\rho U_H(0)L/\mu\f$ with \f$L=1\f$. **Scope limit:** this fully developed channel has
    \f$u\cdot\nabla u=0\f$, so it verifies the analytic scaling across the
    grid but cannot validate an inertial Re transition or the general
    \f$M=Ha/\sqrt{Re}\f$ regime map. That needs a geometry with nonzero
    convective acceleration. The exploratory \f$(Re_H,Ha)=(100,7.9)\f$
    point left the Hartmann branch in this finite duct and is not included in
    the validated interval.
  - **Shercliff 3D duct — done** (Tassone et al. Fig. 6). Full vector field, both feedbacks live, open pressure boundaries with magnetic-pressure profile (form: `MagneticCoupled3D/ShercliffDuct3D/`).
    completion and validation: full vector \f$b=(b_x,0,B_0)\f$, both feedbacks
    live, open pressure boundaries with an imposed magnetic-pressure profile.

### C3 — advanced coupled features

| Item | Note |
|---|---|
| \f$\nabla\cdot b = 0\f$ by vector potential | still open; the opt-in cell-centred projection is implemented separately (see section 3) |
| Effective turbulent diffusivity \f$\eta_t\f$ | implemented from supplied fields, magnetic k/ε fields, or hydraulic k-ε; a 3D VKS turbulent case exists |
| Magnetic fluctuation transport | scaffolding only; transport equations, production/dissipation and wall treatment remain open |
| Turbulent MHD closure | MHD modifications to k-ε (Tassone et al. §4) remain open |
| Energy equation with Ohmic heating | \f$J^2/\sigma\f$ source |
| Lid-driven cavity with transverse \f$B\f$ | standard benchmark |
| Pump-induced MHD duct flow | industrial liquid-metal application |

### C4 — consolidation

Sign conventions, boundary conditions and matrix properties were reviewed and
documented in `KERNEL_REUSE_NOTES.md` §22 (2026-10-24). The Lorentz force is
$\mathbf{J}\times\mathbf{B}$ (per unit mass), the magnetic-pressure
profile is required at open boundaries, and the explicit lagged source does not
enter the Jacobian, so matrix symmetry stays a kernel-level property. The
wall-gradient bias of `induction_stretching` remains a documented limitation.
Still open: a full-kernel ASan/UBSan pass (the module-only environment is not
configured here; the recipe is in `KERNEL_REUSE_NOTES.md` §18.2) and keeping
`KERNEL_REUSE_NOTES.md` / the validation docs aligned as the implementation
evolves.

### Planned coupled validation targets

| Case | Reference | Quantities compared |
|---|---|---|
| Hartmann channel flow | Davidson §3.3 | \f$u_x(y), b_x(y), \mathrm{Ha}\f$ |
| Shercliff 3D duct | Shercliff 1953; Tassone et al. | \f$u(y), b(y)\f$, Re, Ha, M |
| Lid-driven cavity | standard benchmark | \f$u_{x,\max}, b_z\f$ with transverse \f$B\f$ |
| LM breeding blanket | Tassone et al. 2024 | Ha, Re, Rm, \f$\mathrm{Ha}/\sqrt{\mathrm{Re}}\f$ scan |
| Ponomarenko dynamo | Gérard-Varet §2 | kinematic dynamo growth rate |

---

## 5. The long view — VKS and the dynamo regime {#TrustLiMe_plans_vks}

`END_GOAL.md` targets the **CEA VKS experiment** (Von Kármán Sodium): two
counter-rotating impellers in liquid sodium, magnetic Reynolds numbers up to
\f$\mathrm{Rm}\sim 1000\f$ and \f$\mathrm{Pm}\sim 10^{-5}\f$ — a high-Rm,
low-Pm dynamo. The gap from the current validated mode:

| Today | VKS needs |
|---|---|
| low-Rm perturbation \f$(b \sim \mathrm{Ha}^2)\f$ | high-Rm induction, dynamo threshold and saturation |
| lagged two-way coupling (low-Rm, staggered) | fully nonlinear high-Rm coupling |
| molecular plus effective \f$\eta_t\f$ from supplied or hydraulic k-\f$\varepsilon\f$ fields | magnetic fluctuation transport and a calibrated turbulent-EMF closure |
| laminar/prescribed-flow validation, plus a VEF turbulent coupling case | validated turbulent MHD (k-\f$\varepsilon\f$, LES, RSM) |
| forms running at 2 ranks | large 3D spherical meshes, impeller forcing |

Roadmap phases (month ranges are `END_GOAL.md` estimates):

1. **Code preparation (0–6)** — two-equation coupled mode, impeller rotation
   BCs, temperature-dependent sodium properties, spherical-shell meshing.
2. **Baseline simulations (6–12)** — decaying fields below threshold, VKS
   start-up validation, mesh convergence, turbulence-model sensitivity.
3. **Dynamo threshold (12–18)** — Rm scan across \f$\mathrm{Rm}_{\mathrm{crit}}\f$,
   cross-code threshold comparison (TRUST / trust-LiMe / TrioCFD).
4. **Fully developed dynamo (18–24)** — saturated state, field statistics,
   comparison with VKS measurements.
5. **Integration and publication (24–30)** — combined report, code releases,
   simulation database.

@warning A turbulent magnetic diffusivity path exists, including a hydraulic
k-\f$\varepsilon\f$ handoff, but it is not yet a validated high-\f$Rm\f$
closure. Magnetic fluctuation transport equations, broader turbulent
benchmarks, large-scale VKS cases and dynamo threshold/saturation remain future
work. Do not equate the VEF smoke/validation case with a validated VKS simulation.

---

## 6. Settled decisions

These are **settled** — do not re-open them casually; update the plan documents
first if one must change.

| Id | Decision |
|---|---|
| D1 | Problem families: `Pb_Induction` for standalone induction; `Pb_MHD` and `Pb_MHD_Turbulent` for momentum + magnetic; `Pb_MHD_Thermal` adds temperature (NS, temperature, magnetic). Equation indices are part of the contract. |
| D2 | Discretisation: `Polymac_p0` baseline; `Polymac_p1` or edge elements only where \f$\nabla u\f$ / \f$J\times B\f$ resolution or div-b enforcement demands it. |
| D3 | Magnetic diffusivity \f$\eta\f$ read as the equation keyword `diffusivite_magnetique`; no new milieu property. |
| D4 | Boundary conditions reuse kernel scalar CLs (via the `"Thermique"` application domain); magnetic-named vocabulary added only where implemented (`paroi_isolante_magnetique`). |
| D5 | Vector unknown: `nb_composantes` components transported independently as scalars (`Hartmann1D` exercises 2). |
| D6 | Regime: low-Rm — both `induction_stretching` and `force_lorentz` are explicit with lagged fields, nothing in the Jacobian. |

---

## 7. Standing risks

| Risk | Mitigation |
|---|---|
| `-Werror` with `-Wconversion` (implicit \f$\mathrm{double}\to\mathrm{int}\f$, sign mismatch) | fix at first compile, never defer |
| P0 under-resolves \f$\nabla u\f$ and \f$J\times B\f$ | P0-friendly analytic profiles or refined meshes; `Polymac_p1` option later |
| Lorentz-force sign error | unit tests against the analytic Hartmann profile before enabling |
| Loose-coupling instability | same implicit sub-step for both equations when coupled |
| Matrix symmetry broken by \f$J\times B\f$ | verify with the kernel's non-symmetric solver flags when enabled |
| Div-b consistency | the \f$\Delta b\f$ discretisation must preserve constant modes (zero eigenvalue of the diffusion operator) |

---

## 8. Planning documents index

| Document | Content |
|---|---|
| `PLAN_magnetic_standalone.md` | original M0→M1 plan — all phases complete |
| `PLAN_magnetic_standalone_improved.md` | improved plan: vector unknown, sign conventions, div-b groundwork, Lorentz skeleton, analytic validation suite |
| `PLAN_mhd_coupled.md` | coupled C0–C4 phases, decisions, migration path, timeline |
| `END_GOAL.md` | VKS end goal, governing equations, 0–30-month roadmap |
| `KERNEL_REUSE_NOTES.md` | accumulated kernel knowledge — the recipes the plans rely on |
| `AGENTS.md` | "Known gaps / open items" — the short list kept current per session |

@note The plan documents are frozen at the time they were last edited; when a
statement here and a statement there disagree, the repository state — as
checked by the validation suite — wins.
