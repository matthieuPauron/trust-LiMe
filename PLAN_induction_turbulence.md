# Plan — Induction turbulence (k-ε) for trust-LiMe

Status: **Phases 1, 2a, 2b, 2b+ implemented; Phase 4 (hydraulic k-eps coupling)
implemented and exercised end-to-end (grab + non-uniform eta_t); Phase 3
(magnetic fluctuation transport) scaffolded.**

## 1. Motivation

The induction equation currently uses molecular magnetic diffusivity η only.
At mid/high Rm in turbulent flow (VKS-like, Rm ~ 100–1000, Re ~ 1e5–1e6,
Pm ~ 1e-5) the unresolved fluctuations u'×b' act as an extra diffusivity
η_t. Without it the effective Rm is far too high and the field grows
unphysically. The turbulent EDM term is

    ∇×(ū'×b̄') ≈ −η_t ∇×B̄,     η_t = ν_t / Pm_t  (or C_μ^m k²/ε)

mirroring the hydrodynamic k-ε eddy viscosity ν_t = C_μ k²/ε.

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

The induction analogue needs the same machinery but for the magnetic field:
transport equations for the magnetic fluctuation energy (k_b) and its
dissipation (ε_b), plus an eddy magnetic diffusivity η_t.

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

### Phase 3 — transport equations for magnetic fluctuations
- New equation `Convection_Diffusion_K_Magnetic` (or reuse the kernel's
  `Transport_K_Eps` machinery) for k_b, ε_b, coupled to the induction
  equation via the EDM term. This mirrors triocfd's `Transport_K_Eps`.
- Wall treatment and limiters analogous to `calculate_limit_viscosity`.

### Phase 4 — coupling with hydraulic k-ε
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
  the magnetic equation. Two limits found on the way:
  - `Op_Diff_VEF_Face::ajouter_cas_multi_scalaire` aborts for a **vector**
    magnetic unknown (`nb_composantes 2`) on VEF, so a turbulent case must
    either stay scalar or move to PolyMAC;
  - PolyMAC cannot host k-eps here: `Turbulence_paroi_base` builds the wall-law
    type name as `negligeable_<discretisation>` and no `*_PolyMAC_*` wall law
    is registered, so `turbulence_paroi negligeable` fails to type.
  Promoting this to a validation form therefore needs either a PolyMAC wall
  law upstream or a VEF-compatible multi-component diffusion path — module
  code is not the blocker.
- The vector (2-component) path therefore stays on the laminar PolyMAC forms;
  the k-eps grab itself is component-agnostic.
- **VKS 3D turbulent case** (2026-10-23): new form
  `MagneticCoupled3D/VKSTurbulent/` — VEFPreP1B on a tetrahedral mesh
  (`Tetraedriser` + `VerifierCoin`), `Pb_MHD_Turbulent` with `K_Epsilon`,
  `nb_composantes 3`, one-way coupling (no `force_lorentz` — VEF has no
  implementation yet). Runs to completion and prints the grab + non-uniform fold.
  Two-way coupling on VEF requires a VEF `force_lorentz` (new class).
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
