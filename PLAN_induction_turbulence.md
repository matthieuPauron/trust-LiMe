# Plan — Induction turbulence (k-ε) for trust-LiMe

Status (2026-10-24): **Prescribed and k-ε-derived effective magnetic
diffusivity implemented; hydraulic k-ε handoff is exercised end-to-end.** The
3D `VKSTurbulent` case now exercises two-way coupling with VEF `force_lorentz`.
Magnetic fluctuation transport remains scaffolded; high-Rm/VKS dynamo behavior
is not validated. See `docs/content/plans.md` for the cross-project roadmap.

## 1. Motivation

The baseline induction equation uses molecular magnetic diffusivity η; the
module now also supports an effective η + η_t from supplied fields or hydraulic
k-ε fields. At mid/high Rm in turbulent flow (VKS-like, Rm ~ 100–1000, Re ~ 1e5–1e6,
Pm ~ 1e-5) the unresolved fluctuations u'×b' act as an extra diffusivity
η_t. Without an appropriate η_t, the effective Rm is too high and the field
may grow unphysically. The turbulent EDM term is

    ∇×(u'×b̄') ≈ −η_t ∇×B̄,     η_t = ν_t / Pm_t

for the hydraulic handoff. A separate magnetic-fluctuation closure could use
the k-ε form `eta_t = C_mu_b * k_b^2 / epsilon_b / Pm_t_b`, provided `k_b`
is defined in velocity-squared units (see the draft below); it is not the
hydraulic `k` and `epsilon` under different names.

## 2. Reference: triocfd k-ε (read 2026-10-22)

TrioCFD (`/home/matthieu/Development/triocfd-code`) implements RANS k-ε via:

- `Modele_turbulence_hyd_K_Eps` (`src/Turbulence/ThHyd/Modeles_Turbulence/RANS/Hydr/`)
  - transport equations for k and ε (`Transport_K_Eps`), solved as extra
    equations of the hydraulic problem;
  - turbulent viscosity ν_t = C_μ k²/ε (with low-Re corrections via
    `Modele_Fonc_Bas_Reynolds`);
  - `calculer_viscosite_turbulente()` fills a face/element field consumed
    by the momentum diffusion operator;
  - wall laws (`loi_paroi`) and limiters (`calculate_limit_viscosity`).

The induction model can reuse TRUST's scalar transport infrastructure, but it
must not copy the hydraulic production terms blindly: its state variables,
energy transfers, units and closure assumptions are magnetic-specific. The
draft below chooses a minimal EDM-consistent source for a first model and
records the alternative stretching term already present in the code shells.

## 3. Phases

### Phase 1 — constant turbulent magnetic diffusivity (IMPLEMENTED 2026-10-22)
- Add keyword `diffusivite_turbulente_magnetique` (scalar or `Champ_Uniforme`)
  to `Convection_Diffusion_Magnetic_field`, summed with the molecular η in
  `diffusivite_pour_transport()` and `diffusivite_pour_pas_de_temps()`.
- This is a proxy for η_t: a prescribed constant, useful for sensitivity
  studies and for validating the diffusion path with a larger effective η.
- Validation: a 2D case comparing molecular-only vs η+η_t decay (must match
  the analytic exp(-(η+η_t) k² t)).

### Phase 2a — non-uniform η_t folding (IMPLEMENTED 2026-10-22)
- `diffusivite_turbulente_magnetique` may now be any `Champ_Don_base` field
  (uniform or non-uniform). Uniform fields are folded at read time (Phase 1);
  non-uniform fields are summed point-wise in `completer()` into a
  `Champ_Fonc_Somme` (`src/Champ_Fonc_Somme.{h,cpp}`), so
  \f$\eta_{\rm eff}(x) = \eta(x) + \eta_t(x)\f$. Operands are observed
  (`OBS_PTR`), so a \f$\eta_t\f$ that is itself a `Champ_Fonc` is tracked.
- This provides the *interface* for a future k-ε model feeding \f$\eta_t\f$.

### Phase 2b — η_t = C_μ·k²/ε/Pm_t from k, ε fields (IMPLEMENTED 2026-10-22)
- New class `Champ_Fonc_Eta_Turbulent` (`src/Champ_Fonc_Eta_Turbulent.{h,cpp}`):
  P0 element field evaluating `C_mu*k^2/eps/Pm_t` from two operand fields,
  with a floor on eps (1e-12) and operand refresh in mettre_a_jour.
- New keywords on `Convection_Diffusion_Magnetic_field`: `k_magnetique`,
  `epsilon_magnetique` (fields), `C_mu_magnetique` (default 0.09),
  `prandtl_turbulent_magnetique` (default 1.0). In completer(), when both
  k and epsilon are given, the class is built and stored as
  `diffusivite_turbulente_magnetique_`, so the Phase 2a fold handles the rest.
- Constant C_mu for now (realizable variant deferred).

### Phase 3 — transport equations for magnetic fluctuations (SCAFFOLDED)
- Existing `Convection_Diffusion_{K,Epsilon}_Magnetic`,
  `Terme_Source_{K,Epsilon}_Magnetic` and
  `Modele_turbulence_magnetique` are compile-only shells. Implement the two
  scalar equations from the model draft below; reuse generic TRUST scalar
  transport infrastructure only where its lifecycle/operator contracts fit.
- First supported scope is interior/closed-domain transport with positivity
  control. Wall functions, low-Re corrections and broad discretization support
  are later work, not part of the initial equation implementation.

#### Draft model: independent magnetic-fluctuation k-ε

This is a proposed **separate, opt-in closure**, not the implemented hydraulic
k-ε handoff. It evolves two scalar fields, `k_b` and `epsilon_b`, on the
magnetic equation's domain and with its bound transport velocity. It supplies
an isotropic turbulent magnetic diffusivity to the mean induction equation.
It does not replace that equation and, by itself, does not model an alpha
effect, magnetic helicity, or prove a self-sustained dynamo.

##### 3.a State, units and scope

For constant reference density, define the Alfvén-normalized field
`b_A = B / sqrt(mu0 * rho_ref)` and
`k_b = 1/2 <|b_A'|^2>`. Then `k_b` has units m²/s² and `epsilon_b`, its
dissipation rate, has units m²/s³. This is required for
`C_mu_b*k_b²/epsilon_b` to have diffusivity units. The first version should
therefore either establish this normalization at the magnetic-model boundary
or reject unsupported unit/density configurations. Applying that closure
directly to a Tesla-valued field is dimensionally wrong. Variable density and
the mapping to the module's stored magnetic-field units remain an explicit
design gate, not an implicit conversion.

The model is an isotropic, local, two-equation RANS closure. It does not
resolve `b'` or `u'` and must not be described as a direct simulation of
magnetic fluctuations. The hydraulic `k` and `epsilon` remain a distinct
alternative provider of `eta_t`; they are not the model's `k_b` and
`epsilon_b`.

##### 3.b Proposed closure equations

Let `U` be the velocity transported by the induction equation; `eta` the
molecular magnetic diffusivity; and `P_b` the modeled transfer into unresolved
magnetic-fluctuation energy. The first implementation proposal is

```text
∂t k_b + U·∇k_b = ∇·[(eta + eta_t/sigma_kb) ∇k_b] + P_b - epsilon_b

∂t epsilon_b + U·∇epsilon_b =
    ∇·[(eta + eta_t/sigma_epsb) ∇epsilon_b]
    + C_eps1b (epsilon_b/k_b) P_b - C_eps2b epsilon_b²/k_b

eta_t = C_mu_b k_b² / (epsilon_b Pm_t_b)
P_b   = eta_t |curl(b_A)|²
```

The production term is selected because it is non-negative and represents the
loss of resolved mean-field energy caused by the same `eta_t` used in the
induction equation. With closed/no-flux boundaries, it gives a testable energy
transfer path: resolved magnetic energy lost to turbulent diffusion is
injected into `k_b`, then removed by `epsilon_b`. Its units are m²/s³. The
evolution must use a curl-form induction diffusion operator (or demonstrate
the equivalent energy identity for the selected discretization) for this
interpretation to hold, especially with non-uniform `eta_t`.

There is a competing sketch in the current source shells: `Terme_Source_K_Magnetic`
mentions a stretching production proportional to `-b_i b_j d_i u_j`. That is
not the selected v1 expression and its sign/index convention is not yet
specified. A velocity-strain production requires a closure for anisotropic
magnetic correlations; it must not be added on top of `P_b` without deriving
the energy budget, or production may be double-counted. Before implementing
the shells, reconcile/update their comments and test whichever production is
chosen. V1 deliberately omits this extra term; a later model may add it as a
separately named and validated closure option.

The coefficients `C_mu_b`, `C_eps1b`, `C_eps2b`, `sigma_kb`, `sigma_epsb`,
and `Pm_t_b` belong to the magnetic model. They must be named separately
from hydraulic constants and have documented defaults only after calibration.
Do not claim universal values by copying the hydraulic k-ε constants. The
laminar limit is `eta_t -> 0` as `k_b -> 0`; a zero-energy state must remain
well-defined numerically rather than evaluate `0/0`.

##### 3.c Transport meaning and numerical update

`k_b` and `epsilon_b` are **two additional scalar unknowns**, not components
of the vector `b`. Both are advected by the same `U` as the magnetic field.
Their modeled scalar diffusivities are respectively
`eta + eta_t/sigma_kb` and `eta + eta_t/sigma_epsb`; they are not the hydraulic
momentum diffusivity `nu + nu_t/sigma`. The `eta` term is molecular magnetic
diffusion, while the `eta_t/sigma_*` terms model transport of the two scalar
quantities. Convection and diffusion operators must be selected and verified
for each supported discretization rather than inferred from the vector-field
operator.

The initial algorithm is a lagged, staggered update per physical step:

1. Use the current mean `b_A`, `U`, `k_b`, and `epsilon_b` to evaluate bounded
   `eta_t` and cellwise `P_b` from the discrete curl of `b_A`.
2. Advance the `k_b` and `epsilon_b` transport equations with those lagged
   coefficients/sources. Treat destruction terms implicitly where the TRUST
   source API permits; keep source signs and any linearization explicit in the
   implementation notes.
3. Refresh `eta_t` from the updated turbulence fields and solve the induction
   equation with `eta_eff = eta + eta_t`.
4. If `couplage_fort` is enabled, repeat the turbulence/induction sweep until
   the existing iteration tolerance is met; otherwise retain the lagged
   one-sweep behavior.

This ordering makes the closure's feedback visible and avoids an unplanned
fully coupled nonlinear solve. The source and diffusivity fields must be
refreshed after their operands change; a stale `Champ_Fonc` is a correctness
failure. Compute/project `|curl(b_A)|²` at the scalar equation's locations
using the same discrete gradient/curl data as the induction operator where
possible, so the production and mean-field diffusion use compatible operators.

##### 3.d Positivity, initialization and boundaries

Use strictly positive numerical denominator floors
`k_b_eff = max(k_b, k_b_min)` and
`epsilon_b_eff = max(epsilon_b, epsilon_b_min)` in the closure and epsilon
source. Floors are safeguards, not physical sources: do not replace the
transported unknown silently, and report counts/minima when clipping occurs.
Enforce non-negative `k_b` and `epsilon_b` after updates using a documented
TRUST-compatible limiter; test conservation/energy effects of clipping.
`eta_t` also needs a configurable upper bound or a fatal diagnostic for
non-finite/unreasonably large values, rather than allowing the time step or
linear system to fail opaquely. The floor and cap values need unit-aware
defaults and should be exposed only when there is a defensible user need.

For initialization, require both fields or provide an explicit model default;
never read uninitialized transport unknowns. Missing `epsilon_b` with a
positive `k_b` is an input error unless the user supplies a documented length
scale `ell_b` from which the model computes
`epsilon_b = C_mu_b^(3/4) k_b^(3/2) / ell_b`. This optional inlet/initialization
relation must not be confused with solving epsilon's transport equation.
Restart files must save and restore both unknowns and model constants needed
to reproduce `eta_t`.

The first validation scope should use closed domains with zero normal flux for
`k_b` and `epsilon_b`. Inlets need prescribed non-negative turbulence data;
outlets need a tested convective/outflow treatment. Wall functions, conducting
wall production/suppression, and magnetic boundary-layer modeling are deferred.
Do not advertise a wall-bounded turbulent induction case until those choices
and discretization support exist.

##### 3.e TRUST ownership and input contract

`Modele_turbulence_magnetique` should own the two unknowns, transport
equations, constants, floors/limiters, and derived `eta_t`. The existing
`Convection_Diffusion_K_Magnetic`,
`Convection_Diffusion_Epsilon_Magnetic`, `Terme_Source_K_Magnetic`, and
`Terme_Source_Epsilon_Magnetic` files are compilable shells only; they need
real equation/source base classes, read/discretize/update lifecycle, unknown
registration, and tests. Reuse generic TRUST scalar transport infrastructure
where compatible, but do not force the magnetic model into hydraulic
`Transport_K_Eps` ownership or share its hydraulic source terms.

Implementation sequence:

1. Give each `Convection_Diffusion_*_Magnetic` class a genuine scalar equation
   base, unknown, convection operator bound to the induction velocity,
   diffusivity field, boundary conditions, and source term. The model object
   coordinates their read/complete/discretize/update calls and exposes
   references to `k_b`, `epsilon_b`, and `eta_t`.
2. Confirm how model-owned equations enter TRUST's solve, timestep, restart and
   post-processing lifecycle. Use the kernel-supported equation/model hook;
   never advance the unknowns by an ad-hoc call from the induction equation.
   Preserve the public problem ordering contract (momentum at index 0 and
   magnetic at index 1 in `Pb_MHD_Turbulent`). If TRUST requires the transport
   equations to be registered in the problem equation list, register them
   without changing those indices or the meanings of existing datasets.
3. Implement and unit-test the shared `eta_t`/`P_b` evaluator first, including
   operand observation/update and consistent cell-to-equation projection.
   Then implement the k source, epsilon source and scalar equation assembly;
   test each source's `calculer` and `ajouter` signs independently.
4. Wire the lagged timestep sequence and diagnostics only after the two
   equations solve independently. Emit min/max of `k_b`, `epsilon_b`, `eta_t`,
   integrated `P_b`, and the number of limited cells at a configurable
   diagnostic frequency; these are needed to distinguish a closure failure
   from an induction-operator failure.

Conceptual input shape (illustrative only; not valid TRUST syntax until the
classes and keywords are implemented):

```text
modele_turbulence_magnetique {
    C_mu_b ...
    C_eps1b ...
    C_eps2b ...
    sigma_kb ...
    sigma_epsb ...
    Pm_t_b ...
    transport_k_b { ... initial/boundary data ... }
    transport_epsilon_b { ... initial/boundary data ... }
}
```

Both equations must have compatible time schemes and use the same physical
time step as induction. Their initial and boundary data are first-class input
requirements, not values inferred from the hydraulic turbulence model.

Provider contract for the new model: it must not be combined with another
provider of `eta_t`. Preserve legacy behavior for existing inputs: an explicit
`diffusivite_turbulente_magnetique` currently takes precedence over explicit
operand fields; a complete explicit `k_magnetique`/`epsilon_magnetique` pair
builds `eta_t` when no direct field is set; hydraulic fallback is considered
only when none of these inputs is present. The new model keyword should be
mutually exclusive with these providers and fail clearly on conflicts or
incomplete magnetic field pairs, without silently changing old datasets. The
magnetic model must require the same valid transport velocity as induction.
Its scalar equations can share the mesh but need independently verified
scalar convection/diffusion operators; they are not guaranteed by support for
the vector magnetic equation. PolyMAC wall-law limitations remain a blocker
for wall-bounded cases, not something this model should mask.

##### 3.f Verification and validation ladder

1. **Algebra/unit tests:** verify the units/normalization contract, closure
   `eta_t`, floors, caps, zero/near-zero limits, and source signs; reject NaN,
   negative inputs and incomplete/conflicting providers.
2. **Transport-only tests:** with `P_b=0`, verify non-negative k/epsilon,
   advection, diffusion and epsilon decay on a simple domain, including mesh
   and time-step refinement.
3. **Production test:** prescribe a curl-bearing mean field and verify the
   discrete `P_b` field against `eta_t*|curl(b_A)|²`; a uniform field must give
   zero production. Confirm source-unit and projection consistency.
4. **Energy transfer test:** in a closed domain, measure mean magnetic energy,
   integrated `P_b`, `k_b`, and dissipation. Check that the resolved-field
   turbulent-diffusion loss and turbulence production balance to the expected
   time/discretization error; report boundary fluxes separately.
5. **Coupled regression:** demonstrate that changing `k_b`/`epsilon_b` changes
   `eta_eff` and the induction decay rate; compare molecular-only, prescribed
   `eta_t`, hydraulic fallback, and magnetic-model runs without expecting the
   two turbulence closures to match pointwise.
6. **Physical calibration:** only after the above, calibrate coefficients
   against a suitable magnetic-turbulence/VKS reference and add a high-Rm
   validation case. `VKSTurbulent` currently validates hydraulic k-ε reuse,
   not this independent model.

Do not mark Phase 3 implemented or call the closure validated until the source
shells, restart, positivity behavior, energy-transfer check, and at least one
mesh/time refinement test pass.

### Phase 4 — coupling with hydraulic k-ε (IMPLEMENTED)
- In the coupled mode (`vitesse_hydraulique`), reuse the hydraulic k, ε to
  compute η_t = ν_t / Pm_t instead of solving separate magnetic fluctuation
  equations. This is the VKS-relevant path.

## 4. Schemes for turbulent induction

- Time: `Schema_Euler_Implicite` (already used) — turbulent diffusion is stiff;
  implicit avoids dt restrictions. Keep `nb_ite_max` high enough.
- Convection: `amont` (upwind) for stability at high Rm/Peclet; the laminar
  cases use `amont`. Consider `muscl` for accuracy once stable.
- Diffusion: standard (PolyMAC MPFA). The effective η = η + η_t lowers the
  cell Peclet, improving stability.
- Coupling: keep the lagged staggered scheme by default; use `couplage_fort`
  (Phase 1 strong coupling) when turbulent terms are active.

## 5. High-Rm case update

- `HartmannTwoWay` / a new `HighRmInduction` case: run with η_t > 0 and
  verify the field decays/grows at the effective rate; compare to the
  molecular-only run. Expected: with EDM the effective Rm drops, suppressing
  spurious growth.

## 6. Files to touch

- `src/Convection_Diffusion_Magnetic_field.{h,cpp}`: keyword + diffusivity sum.
- `src/Terme_Source_*` (optional): EDM as a source if not folded into diffusivity.
- Validation: `MagneticStandalone/TurbulentDiffusion/` (2D) + 3D twin.
- `AGENTS.md`, `KERNEL_REUSE_NOTES.md`, `docs/content/model.md`.
### Phase 4 — hydraulic k-ε coupling (IMPLEMENTED 2026-10-22)
- The hydraulic k-ε model is carried by `Navier_Stokes_turbulent`, which is
  equation 0 of `Pb_MHD_Turbulent` (a `Pb_Hydraulique_Turbulent`); the
  magnetic equation is equation 1. This is what the hierarchy refactor
  (§20 of `KERNEL_REUSE_NOTES.md`) made possible — the old `Pb_MHD` had no
  turbulent momentum equation at all.
- In `completer()`, when `vitesse_hydraulique` is set and no explicit k/eps
  fields are given, grab the momentum equation's turbulence model fields
  (`k`/`epsilon` if the model exposes them, else the 2-component `K_Eps`
  transport field, comp 0 = k, comp 1 = eps) and build the
  `Champ_Fonc_Eta_Turbulent` from them:
  `eta_t = C_mu*k^2/eps/Pm_t`, logged as
  `hydraulic turbulence model detected, using its k/epsilon for the magnetic
  eddy diffusivity.`
- The field is non-uniform, so it goes through the Phase 2a point-wise fold
  (`eta_eff = eta + eta_t`, logged as `turbulent magnetic diffusivity folded
  (non-uniform) into eta_eff = eta + eta_t`).
- **Kernel-side trap found and guarded** (2026-10-22):
  `Modele_turbulence_hyd_K_Eps::preparer_calcul()` (and the `_Bicephale`
  twin) unconditionally did
  `equation(1).get_modele(TURBULENCE).valeur()` whenever the problem has more
  than one equation, assuming equation 1 is the scalar (thermal) transport
  equation. In `Pb_MHD_Turbulent` equation 1 is the magnetic equation, which
  has no turbulence model, so the null reference aborted. Both call sites now
  test `modele_turbulence` before dereferencing it (change in
  `../triocfd-code`, outside this repo's diff — same precedent as the
  PolyMAC guards).
- Validation status (2026-10-22): a 2D smoke case runs to completion
  (`Finished correctly`) and prints
  `eta_t in [0.45, 0.675]` — i.e. the hydraulic k, eps give a genuinely
  non-uniform eddy diffusivity — followed by the `eta_eff = eta + eta_t` fold.
  Working recipe: `VEFPreP1B` on a **triangulated** mesh
  (`Trianguler dom` + `VerifierCoin dom { }`), `Navier_Stokes_turbulent` with
  `K_Epsilon { turbulence_paroi negligeable ... }`, and `nb_composantes 1` on
  the magnetic equation. PolyMAC still cannot host k-eps here:
  `Turbulence_paroi_base` builds the wall-law type name as
  `negligeable_<discretisation>` and no `*_PolyMAC_*` wall law is registered,
  so `turbulence_paroi negligeable` fails to type. Broader turbulent
  discretization coverage remains an upstream wall-law concern.
- **VKS 3D turbulent case** (2026-10-23, updated 2026-10-24): form
  `MagneticCoupled3D/VKSTurbulent/` — VEFPreP1B on a tetrahedral mesh
  (`Tetraedriser` + `VerifierCoin`), `Pb_MHD_Turbulent` with `K_Epsilon`,
  `nb_composantes 3`, and two-way coupling through `vitesse_hydraulique` plus
  the VEF `force_lorentz` assembly. The notebook exercises the turbulent
  diffusivity handoff and both coupling directions; this is a targeted
  integration case, not a high-Rm dynamo validation.
- Minimal working data-file skeleton for the smoke case (2D, 0 <= x <= 10,
  0 <= y <= 1, 11x6 nodes triangulated):

  ```
  dimension 2
  Pb_MHD_Turbulent pb
  Domaine dom
  Mailler dom { Pave pave { Origine 0. 0.  Nombre_de_Noeuds 11 6  Longueurs 10. 1. } {...} }
  Trianguler dom
  VerifierCoin dom { }
  VEFPreP1B dis
  ...
  Read pb
  {
      nb_composantes 1
      fluide_incompressible { mu Champ_Uniforme 1 1e-2  rho Champ_Uniforme 1 1. ... }
      Navier_Stokes_turbulent
      {
          solveur_pression Petsc Cholesky { }
          ...
          modele_turbulence K_Epsilon
          {
              turbulence_paroi negligeable
              Transport_equation transport_k_epsilon
              {
                  ...
                  initial_conditions { k_Eps Champ_fonc_txyz dom 2 5.*(1.+0.5*y) 11.25 }
              }
          }
      }
      Convection_Diffusion_Magnetic_field
      {
          diffusivite_magnetique Champ_Uniforme 1 0.05
          vitesse_hydraulique
          ...
      }
  }
  Solve pb
  ```

  `turbulence_paroi negligeable` must appear on the **hydraulic** model too
  (it is a REQUIRED keyword there), otherwise TRUST stops with
  `turbulence_paroi of Modele_turbulence_hyd_K_Epsilon has not been read while
  it is required`.

### Phase 3 — magnetic fluctuation transport (SCAFFOLDED 2026-10-22)
- `Modele_turbulence_magnetique`, `Convection_Diffusion_K_Magnetic`,
  `Convection_Diffusion_Epsilon_Magnetic`, `Terme_Source_K_Magnetic`,
  `Terme_Source_Epsilon_Magnetic` — compilable shells with TODOs.
- Physics (convection, diffusion, production, dissipation, wall functions)
  deferred.
