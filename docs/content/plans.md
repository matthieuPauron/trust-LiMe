@page TrustLiMe_plans Roadmap and Next Steps

This page condenses the project's planning documents — the `PLAN_*.md` and
`END_GOAL.md` files at the repository root — into one view of what is **done**,
what comes **next**, and what is deliberately **deferred**. The plain-Markdown
originals remain the working documents: they are updated as work proceeds, and
this page follows them.

@note Status snapshot: October 2026. Every item marked *done* below is backed
by a green run — see @ref TrustLiMe_validation "Validation cases" for the
36-form suite that guards it (every positive form with its mesh-convergence scan on the coarse/standard/fine ladder), and @ref TrustLiMe_testcases "Test cases" for
the unit-test layer underneath.

---

## 1. Where the project stands

| Milestone | Status |
|---|---|
| M0 — standalone magnetic advection–diffusion | **done** |
| M1 — \f$S = (b\cdot\nabla)u\f$ stretching source (`induction_stretching`) | **done** |
| Coupled low-Rm mode — `force_lorentz` on an optional `Navier_Stokes_standard` | **done** (one-way \f$b \to\f$ momentum) |
| Post-treatment fields (`Champ_Fonc_MHD`) and their 12 forms | **done** |
| Coupled validation forms (7) | **done** |
| Full validation suite — 2D, 3D twins, coupled, post-treatment | **done** — 37/37 forms pass (checked 2026-10-21; every baseline at `nbProcs=2`, the §18.2-pinned scan levels — `CourantMagnetique` fine, `Shercliff3D` standard/fine — at 1 rank), every positive form carrying its mesh-convergence scan on the `coarse`/`standard`/`fine` ladder from a single substituted `src/<case>.data` template (suite history: DiffusionRefined/MMS_CoupledRefined absorbed, TwoWayFeedback folded into Coupled_Feedback, the MMS families fused into `MMS_2D`/`MMS_3D`, the malformed-input forms merged into one `ErrorInputs` notebook per category, the Shercliff 3D twin added closing the R5 gap) |
| Unit tests (GoogleTest, arithmetic suite) | **done** — green via `--arith-only` |
| Two-way coupling — NS velocity feeds the magnetic convection (`vitesse_hydraulique`) | **done** (lagged staggered scheme; commits `9e5b13b`, `d484fbd`) |
| \f$\nabla\cdot b = 0\f$ enforcement, turbulence, dynamo | **open** — sections 3–5 |

Both earlier plans are therefore essentially retired: all phases of
`PLAN_magnetic_standalone.md` are complete, and `PLAN_mhd_coupled.md` has its
C0 kernel work plus C1 (both coupling directions) done. The live work is the rest of
the coupled plan, and beyond it the horizon of `END_GOAL.md`.

---

## 2. Roadmap at a glance

| Stage | Content | Status | Plan document |
|---|---|---|---|
| Standalone M0/M1 | induction equation + stretching source | done | `PLAN_magnetic_standalone.md` |
| Coupled C0 | kernel plumbing: optional equations, Lorentz source | done | `PLAN_mhd_coupled.md` |
| Coupled C1 | low-Rm momentum coupling | **done** — one-way `force_lorentz`, two-way `vitesse_hydraulique` | `PLAN_mhd_coupled.md` |
| Coupled C2 | full NS+MHD, Shercliff duct, Ha/Re scan | next | `PLAN_mhd_coupled.md` |
| Coupled C3 | div-b enforcement, turbulent MHD, Ohmic heating | open | `PLAN_mhd_coupled.md` |
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

A few 2D forms have no 3D twin (e.g. `Shercliff2D`), and the `Hartmann1D`
2D/3D forms differ in probe sets by design. Worth a twin sweep once the form
set stabilises — tracked as R5 in `AGENTS.md`.

### \f$\nabla\cdot b = 0\f$ handling

The module assumes a solenoidal field but does **not** enforce it; the
Green-Gauss reconstruction of \f$\nabla b\f$ is exact for linear fields at
interior cells and O(h) at walls (validation probes target interior cells).
Three upgrade paths, in increasing ambition:

1. **Post-hoc correction** at interior cells — lightweight, no new
   infrastructure.
2. **Pressure-correction** projection — machine-precision
   \f$\nabla\cdot b = 0\f$, but requires new solver machinery beyond the
   standalone scope.
3. **Vector potential / edge elements** (Nedelec) — the M3 formulation; it
   also unlocks the missing `helicite_magnetique` and
   `helicite_generalisee` post-treatment fields.

---

## 4. Coupled MHD — the C-phases {#TrustLiMe_plans_coupled}

From `PLAN_mhd_coupled.md`. Both coupling directions are now implemented:
the Lorentz force \f$J\times B\f$ (explicit, lagged \f$b\f$) acts on the
momentum equation, and — with the `vitesse_hydraulique` keyword — the
magnetic equation transports with the **solved NS velocity**
(`induction_stretching` follows it with a per-step grad(u) rebuild).
The time stepping is a lagged staggered scheme,
\f$b^{s+1}=F(b^s,u^s)\f$, \f$u^{s+1}=G(u^s,b^s)\f$.

### C2 — two-way coupling — **done** (2026-10-18)

- The **NS velocity is the magnetic transport velocity** via the
  `vitesse_hydraulique` pathway (bound in `completer()`, once every
  equation is known). Validated by the two-way discriminator form
  (folded into `Coupled_Feedback` during the notebook-merging pass).
- The `Coupled_Feedback` one-way regression and `ForceLorentzBalance`
  stayed green, plus the two error forms `Error_TwoWayNoNS` /
  `Error_TwoWayBothVelocities`.
- Still open (C2 follow-up): tightened time-step synchronisation if the
  explicit staggered scheme ever shows loose-coupling oscillations.
- Add the tracked coupled benchmarks: the **Shercliff 3D duct** (Tassone et
  al. Fig. 6) and a **Ha/Re regime scan**.

### C3 — advanced coupled features

| Item | Note |
|---|---|
| \f$\nabla\cdot b = 0\f$ by vector potential | edge elements / Nedelec (M3) |
| Turbulent MHD | k-\f$\varepsilon\f$ with MHD modifications (Tassone et al. §4) |
| Energy equation with Ohmic heating | \f$J^2/\sigma\f$ source |
| Lid-driven cavity with transverse \f$B\f$ | standard benchmark |
| Pump-induced MHD duct flow | industrial liquid-metal application |

### C4 — consolidation

Sanitizer pass (`project.cfg` ASan/UBSan + `make module_custom`), the review
checklist (sign conventions, BCs, matrix symmetry), and appendices to
`KERNEL_REUSE_NOTES.md` (coupled sign conventions, Ha-Re-M matrix, low-Rm
limits). The remaining effort was estimated at **6–8 days** on the slow
development host (C2: 3–4 d, C3: 2–3 d, C4: 1 d).

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
| one-way coupling \f$b \to\f$ momentum | two-way nonlinear coupling |
| molecular \f$\eta\f$ only | **turbulent EDM** \f$\eta_t\f$ from k-\f$\varepsilon\f$: \f$\nabla\times\overline{u'\times b'} \approx -\eta_t\,\nabla\times\bar{B}\f$ |
| laminar / prescribed mean flow | fully turbulent flow (k-\f$\varepsilon\f$, LES, RSM) |
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

@warning The turbulent-EDM item is the one that most directly changes the
equation solved today: the induction equation currently carries **molecular**
\f$\eta\f$ only (a constant `Champ_Uniforme`), and the whole validation suite
assumes laminar or prescribed-mean-flow velocities. Model turbulent diffusivity
only together with a turbulence closure, not as a standalone patch.

---

## 6. Settled decisions

These are **settled** — do not re-open them casually; update the plan documents
first if one must change.

| Id | Decision |
|---|---|
| D1 | Equation count: 1 standalone; the kernel's optional-equation mechanism appends `Navier_Stokes_standard` for the coupled mode (no three-equation problem yet). |
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
