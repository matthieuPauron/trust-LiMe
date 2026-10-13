# Improved Work Plan — MHD Integration in trust-LiMe baltik

## Overview

This plan describes how to integrate Magnetohydrodynamics (MHD) models described in the
documentation textbooks (Davidson 2001, Gérard-Varet 2006, Tassone et al. 2024) into the
trust-LiMe baltik application. It improves upon both the current implementation and the
existing `PLAN_magnetic_standalone.md` plan by:

- **Proper vector treatment** of the magnetic field `b` (ndim components, not scalar)
- **Consistent sign conventions** matching the MHD literature
- **Groundwork for Lorentz force** coupling (deferred but with infrastructure)
- **Magnetic divergence control** at the discretization level
- **Analytic validation** against classic MHD solutions from the textbooks
- **Extended discretization options** beyond P0 while maintaining P0 fallback

---

## 1. MHD Physics from the Textbooks

### 1.1 Induction Equation (Davidson §2.2, Gérard-Varet Eq. 1)

The fundamental induction equation from incompressible MHD:

```
∂b/∂t + (u·∇)b = (b·∇)u + η Δb          (1)
```

or equivalently (Gérard-Varet, dimensionless form):

```
∂b/∂t − curl(u × b) − (1/Rm) Δb = 0      (1b)
```

where:
- `b` = magnetic field (vector, ndim components)
- `u` = fluid velocity (given data field, not solved here)
- `η = 1/(μ₀σ)` = magnetic diffusivity (κ in some texts)
- `Rm = μ₀σU L` = magnetic Reynolds number

**Key terms**:
- `(u·∇)b` — advection of magnetic field by velocity
- `(b·∇)u` — **magnetic stretching** (the source term for M1)
- `η Δb` — magnetic diffusion

### 1.2 Momentum Equation (with Lorentz force)

```
ρ (∂u/∂t + u·∇u) = −∇p + J × B + μ₀ρg          (2)
```

where `J = curl B / μ₀` and the Lorentz force `J × B` is the key MHD coupling.

**Dimensionless numbers** (Tassone et al. 2024, breeding blanket paper):
- `Ha = B₀ L √(σ / ρ ν)` — Hartmann number (Lorentz/viscous force ratio)
- `Re = v₀ L / ν` — Reynolds number
- `Rm = μ₀ σ v₀ L` — Magnetic Reynolds number
- `Pr = ν / κ` — Prandtl number

**Low-Rm regime** (Tassone et al.): induced field is a perturbation; the `(b·∇)u` source
is explicit with lagged `b`, consistent with kinematic MHD.

### 1.3 Boundary Conditions (Davidson §2.4, §4)

- **Dirichlet**: imposed magnetic field on walls `b = b₀`
- **Insulating wall** (homogeneous Neumann): `∂b/∂n = 0` (zero normal current)
- **Perfect conductor**: `b × n = 0` (tangential field vanishes)

---

## 2. Improvement Areas Over `PLAN_magnetic_standalone.md`

| Aspect | Original Plan | Improved Plan |
|--------|--------------|---------------|
| **Field unknown** | Scalar transport with `ndim` components (`nb_compo = ndim`) | Explicit `Champ_Inc` vector field from the start |
| **Discretization** | `Polymac_p0` only (uniform) | `Polymac_p0` as baseline + optional `Polymac_p1` for M1 validation |
| **Diffusivity** | Keyword `diffusivite_magnetique` | Same keyword, but with `η > 0` validation and Ha-number reporting |
| **Source terms** | M1 only: `(b·∇)u` explicit, lagged `b` | M0: pure diffusion; M1: `(b·∇)u` + optional `(B₀·∇)u` uniform background |
| **BC vocabulary** | `magnetic_field_impose`, `paroi_isolante_magnetique` | Same + `paroi_conductrice_magnetique` (perfect conductor) + `paroi_isolante_magnetique` (Neumann) |
| **Div-b control** | Not enforced (assumed by validation cases) | Consistent mass-matrix treatment; `Δb` discretization preserves constant modes |
| **Lorentz force groundwork** | Out of scope | Add `terme_lorentz` skeleton with `J × B`; enable via keyword `force_lorentz` (off by default) |
| **Analytic validation** | Not specified | Analytic Hartmann/Shercliff profiles from Davidson §3.3, Shercliff 1953 duct experiment |
| **Sign convention** | Ad hoc | Matched to Gérard-Varet: `(u·∇)b - (b·∇)u - ηΔb = 0` |

---

## 3. Phase Plan — Improved

### Phase 0 — Baseline Compiles (as before)
- [ ] Fix `src/Convection_Diffusion_Magnetic_field.h` (include, syntax, `};`)
- [ ] Fix `src/Pb_MHD.h/cpp` (equation map, `nombre_d_equations()`)
- [ ] Fix `src/Convection_Diffusion_Magnetic_field.cpp` (typos, `readOn`)
- [ ] `syntax-check.sh` clean; `make module_semi_optim` links
- [ ] Smoke run: minimal `Pb_MHD` jdd, pure diffusion, 1 proc

### Phase 1 — Vector M0: Magnetic Scalar Transport (IMPROVED)
- [ ] **1.1 Inconnue**: `OWN_PTR(Champ_Inc_base)` with `nb_compo = ndim` (vector field),
       following `Convection_Diffusion_Concentration` pattern. One discretization.
- [ ] **1.2 Diffusivity**: `η` read as keyword `diffusivite_magnetique`; `η > 0` in `verifier`.
- [ ] **1.3 BCs**: Minimal magnetic BC set:
       - `magnetic_field_impose` → Dirichlet (each component)
       - `paroi_isolante_magnetique` → homogeneous Neumann
       - New: `paroi_conductrice_magnetique` → perfect conductor (`b × n = 0`)
- [ ] **1.4 I/O**: `readOn`/`set_param`/`lire_motcle_non_standard` with magnetic names;
       `solveur_masse->set_name_of_coefficient_temporel("magnetic_field")`
- [ ] **1.5 Discretisation**: `terme_convectif` + `terme_diffusif` wired for vector `b`.
- [ ] **1.6 Assembler**: Per-component assembly, modeled on `Convection_Diffusion_Temperature.cpp`.
       Convection with data `u`, diffusion with η, mass-matrix term.
- [ ] **1.7 Transport velocity as data**: `associer_vitesse` wired to `Champ_Fonc`
       read in jdd (`velocite Champ_Fonc_xyz dom ndim …`). No `Pb_Hydraulique` dependency.
- [ ] **1.8 Rewrite HartmannFlow/src/jdd.data** with new keywords: pure-diffusion 1D slice
       (u = 0), Dirichlet `b = 0` on walls, optional insulating BC.
- [ ] **1.9 Analytic validation**: Compare against 1D decay profile `b(y,t) = b₀ exp(−ηk²t)`,
       analytic Hartmann profile from Davidson §3.3.

**Acceptance**: `syntax-check.sh` clean; module links; Hartmann jdd runs and field
decays toward analytic profile (no convection yet).

### Phase 2 — M0 Validation End-to-End
- [ ] **2.1 Extend HartmannFlow.ipynb**: analytic cells (decay/L2-error/profile), plot b;
       run with `trustutils` (nbProcs=1).
- [ ] **2.2 Create Shercliff_3D_duct/src/jdd.data** (3D, P0, u=0 → isotropic diffusion)
       and its notebook cells.
- [ ] **2.3 Execute both notebooks via `/validate` recipe**; commit reference outputs
       + `tablePerf`.

**Acceptance**: both notebooks execute end-to-end; L2 error below tolerance;
results reproducible.

### Phase 3 — M1: Kinematic Stretching Source (IMPROVED)
- [ ] **3.1 Implement source `S = (b·∇)u`**, explicit (lagged b):
       - `Source_base` subclass pattern (`Terme_Source_Qdm`), or direct second-member
         contribution in `derivee_en_temps_inco`.
       - Keyword `source_etirement { }` to enable (off ⇒ M0 behavior, regression check).
- [ ] **3.2 `∇u` for the data velocity**: Analytic gradient fields supplied in the jdd
       (valid for polynomial/Hartmann profiles). Keep out of coupled-NS path.
- [ ] **3.3 Hartmann 1D verification**: Impose `u_x(y)` analytic Hartmann profile,
       solve for induced `b_x(y)`, compare to analytic solution from Davidson §3.3.
       Verify `source_etirement` off reproduces Phase 2 results (regression check).

**Acceptance**: notebook shows induced-field profile matching analytics within tolerance;
toggling `source_etirement` off reproduces Phase 2 results.

### Phase 4 — Lorentz Force Groundwork (NEW — improves over original out-of-scope)
- [ ] **4.1 Add `terme_lorentz` skeleton** with `J × B` computation:
       - `J = curl b / μ₀` (using same `Polymac_p0` discretization)
       - Lorentz force term added to momentum equation as second member
       - Keyword `force_lorentz { }` off by default
- [ ] **4.2 Update `Pb_MHD::nombre_d_equations()`** to 3 when Lorentz is enabled
       (magnetic + 2 coupled equations, but magnetic equation stays standalone in M0/M1)
- [ ] **4.3 Update docs**: `docs/content/model.md` — mark `(b·∇)u` implemented;
       `force_lorentz` "not implemented" but infrastructure present.

### Phase 5 — Consolidation & Documentation
- [ ] **5.1** Update `docs/content/model.md`: mark M1 implemented; keep Lorentz "not implemented".
- [ ] **5.2** Fill `docs/content/test_cases.md` (M0/M1/Lorentz cases + how to run).
- [ ] **5.3** Sanitizer pass: `project.cfg` ASan/UBSan, `./configure && make module_custom`,
       run M0/M1/Lorentz cases, revert `project.cfg`.
- [ ] **5.4** Review via `.opencode/commands/review.md` checklist (sign conventions,
       BCs, Gauss-law handling, matrix symmetry, Lorentz infrastructure).
- [ ] **5.5** Append findings to `KERNEL_REUSE_NOTES.md`: vector BCs, MHD sign conventions,
       Ha-number post-processing, low-Rm approximation.

**Acceptance**: all notebooks execute; sanitizers pass; documentation complete;
review checklist signed off.

---

## 4. Key Decisions (settled, improving D1–D4)

- **D1 — Equation count**: Magnetic-only `nombre_d_equations() = 1` in M0/M1 phases.
  Lorentz force adds 2 more equations but is off by default (`force_lorentz { }`).
- **D2 — Discretization**: `Polymac_p0` as baseline (D2 from original plan), with optional
  `Polymac_p1` for M1 stretching source when mesh resolves `∇u`. P0-friendly: elementwise
  constant gradients or analytic profiles, else refine mesh.
- **D3 — η source**: Equation keyword `diffusivite_magnetique`. No new milieu property.
  Additionally: `Ha = B₀ L √(σ/ρν)` reported in post-processing for validation.
- **D4 — Boundary conditions, rephrased**: Reuse kernel's existing scalar BC classes under
  magnetic keyword names. New vocabulary: `paroi_conductrice_magnetique` (perfect conductor).
  Physics-specific magnetic BCs (conducting vs. insulating) are now explicitly offered,
  not deferred.

---

## 5. Risks (enhanced from original)

- `-Werror` with `-Wconversion` — implicit `double→int`, sign mismatches in per-component
  loops; fix at first compile. **P1**: use `int->int` casts explicitly where needed.
- **P0 under-resolves `∇u` for stretching source** (M1) — M1 validation profiles must be
  P0-friendly (elementwise-constant or analytic gradients), otherwise refine the mesh.
  **P2**: add `Polymac_p1` option with error estimator for `∇u`.
- **Temperature-base inheritance leaks semantics** — Phase 1 must strip/neutralize them,
  not rename only. **P3**: verify `Convection_Diffusion_Magnetic_field` has zero dependence
  on `rho_cp`, `boussinesq`, or any thermal keywords.
- **4-thread HDD box** — keep the loop `syntax-check.sh → make module_semi_optim → /validate`;
  don't run full `make optim` in this phase.
- **Low-Rm approximation validity** — ensure `source_etirement` `(b·∇)u` term uses the
  correct sign per Gérard-Varet: `∂b/∂t + (u·∇)b - (b·∇)u - ηΔb = 0`. A sign error here
  would flip the stretching direction.
- **Div-b consistency** — the discretization of `Δb` must be consistent with the mass matrix
  so that constant magnetic modes are preserved (zero eigenvalue of diffusion operator).

---

## 6. Integration with Existing TRUST Infrastructure

### 6.1 Keyword Reading Order (from `Pb_MHD::readOn`)
```
Read pb
{
  nb_composantes <ndim>
  fluide_incompressible { ... }
  Convection_Diffusion_Magnetic_field {
    diffusivite_magnetique <η>
    magnetic_field_impose <bc on walls>
    paroi_isolante_magnetique <on walls>
    paroi_conductrice_magnetique <on walls>
    source_etirement <off/on>
    force_lorentz <off/on>
  }
}
```

### 6.2 Velocity Field (data, not solved)
- Read in jdd: `velocite Champ_Fonc_xyz dom ndim composantes …`
- Associated via `associer_vitesse()` to `Convection_Diffusion_Magnetic_field`
- No `Pb_Hydraulique` dependency in M0/M1 phases

### 6.3 I/O Naming Convention
- `solveur_masse->set_name_of_coefficient_temporel("magnetic_field")`
  (mirrors `Convection_Diffusion_Temperature.cpp:59`)
- Output fields named `magnetic_field` (vector, ndim components)

### 6.4 Discretization Operators
- `terme_diffusif` → `Δb` with coefficient η (magnetic diffusivity)
- `terme_convectif` → `(u·∇)b` with data velocity `u`
- `terme_source` → `(b·∇)u` when `source_etirement` enabled
- `terme_lorentz` → `J × B` when `force_lorentz` enabled (same `Champ_Inc` space)

---

## 7. Analytic Validation Suite (from Textbooks)

| Case | Reference | Analytic Solution |
|------|-----------|-------------------|
| **1D magnetic decay** | Davidson §2.3 | `b(y,t) = b₀ exp(−η(k² + n²π²/L²)t)` |
| **Hartmann channel flow** | Davidson §3.3 | `u_x(y) = 1 − cosh(Ha·y/L)/cosh(Ha/2)`, `b_x(y)` from induction eq. |
| **Shercliff 3D duct** | Shercliff 1953, Tassone et al. | Isotropic diffusion `∂b/∂t = ηΔb` with `u = 0` |
| **Ponomarenko dynamo** | Gérard-Varet §2 | Kinematic dynamo with stretch-diffuse mechanism |
| **Lorentz-force-driven flow** | Tassone et al. Eq. 1–4 | `Ha²∇²u − J × B` balance in CMHD |

These validate each phase: M0 (cases 1,2), M1 (case 2 with stretching), Lorentz (case 5).