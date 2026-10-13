# Coupled MHD Simulations Plan — trust-LiMe baltik

## Overview

This plan describes the integration of **fully coupled MHD simulations** into trust-LiMe, going beyond the standalone magnetic induction equation (M0/M1) to solve the complete set of MHD equations where the magnetic field and fluid velocity are solved simultaneously or in a tightly coupled manner.

**Key difference from standalone plan**: Lorentz force feedback is **included**, the momentum equation is solved, and the system is closed. This is "real MHD" as described in Davidson, Gérard-Varet, and Tassone et al.

---

## 1. MHD Governing Equations

### 1.1 Induction Equation (kinematic + dynamic)

```
∂b/∂t + (u·∇)b = (b·∇)u + η Δb          (1)
```

or dimensionless (Gérard-Varet):

```
∂b/∂t − curl(u × b) − (1/Rm) Δb = 0      (1b)
```

### 1.2 Momentum Equation (with Lorentz force)

```
ρ (∂u/∂t + u·∇u) = −∇p + J × B + μ₀ρg       (2)
```

where `J = curl B / μ₀` is the current density and `J × B` is the **Lorentz force**.

### 1.3 Incompressibility

```
∇ · u = 0,     ∇ · b = 0                    (3)
```

### 1.4 Dimensionless Numbers (Tassone et al. 2024)

| Symbol | Definition | Physical Meaning |
|--------|-----------|-----------------|
| `Ha = B₀ L √(σ / ρ ν)` | Hartmann number | Lorentz / viscous force ratio |
| `Re = v₀ L / ν` | Reynolds number | Inertial / viscous forces |
| `Rm = μ₀ σ v₀ L` | Magnetic Reynolds number | Induction / diffusion rate |
| `Pr = ν / κ` | Prandtl number | Momentum / thermal diffusion |
| `M = Ha / √Re` | Interaction parameter | Lorentz / inertial forces |

**Regime**: Low-Rm (typical for liquid metals, Rm ≪ 1) → induced field is a perturbation.

---

## 2. Improvement Over `PLAN_magnetic_standalone.md`

| Aspect | Standalone (M0/M1) | Coupled MHD |
|--------|-------------------|-------------|
| **Unknowns** | Magnetic field `b` only (velocity given) | `b` + `u` both solved (velocity from Navier-Stokes) |
| **Lorentz force** | Out of scope (`force_lorentz { }` off) | **Included**: `J × B` couples momentum → induction |
| **Equation count** | `nombre_d_equations() = 1` | `nombre_d_equations() = 3` (momentum + energy + magnetic) or more |
| **Velocity field** | Data field from jdd (`vitesse_magnetique`) | Field from `Pb_Hydraulique` + MHD coupling |
| **Div-b enforcement** | Not enforced (assumed by validation) | Can be enforced via vector potential or constrained transport |
| **Validation cases** | 7 standalone `.data` files | New coupled cases: Hartmann flow, Shercliff duct, libb flows |
| **Sign convention** | `(u·∇)b - (b·∇)u - ηΔb = 0` | Same, but `(b·∇)u` term fully coupled |

---

## 3. Plan Phases — Coupled MHD

### Phase 0 — Kernel & Infrastructure Upgrades

- [ ] **0.1** Extend `Pb_MHD::nombre_d_equations()` to return 3 when `force_lorentz` enabled
- [ ] **0.2** Add `terme_lorentz` to `Convection_Diffusion_Magnetic_field` or create new `Terme_Lorentz` subclass
- [ ] **0.3** Add `Pb_Hydraulique` coupling infrastructure (momentum equation solve)
- [ ] **0.4** Add `J = curl B` computation using same `Polymac_p0` discretization
- [ ] **0.5** `syntax-check.sh` clean; `make module_semi_optim` links
- [ ] **0.6** Smoke run: minimal coupled jdd (no BCs, u=0 → induced field diffusion only)

**Acceptance**: module compiles; Lorentz force term adds without crashes; pure diffusion case (u=0) verified.

### Phase 1 — C0: Momentum + Magnetic Coupling (Low-Rm)

- [ ] **1.1** Implement `terme_lorentz` with `J × B` using `J = curl b / μ₀`
- [ ] **1.2** Add keyword `force_lorentz { }` (off by default; on → `nombre_d_equations() = 3`)
- [ ] **1.3** Wire Lorentz force as second member in momentum equation
- [ ] **1.4** Update `readOn` / data-file keywords:
       - `force_lorentz <off/on>`
       - `ha_number <value>` (optional, for parameter studies)
- [ ] **1.5** Transport velocity now has two sources:
       - `vitesse_magnetique` (data, as before)
       - `vitesse_hydraulique` from `Pb_Hydraulique` (when coupled)
- [ ] **1.6** Hartmann 1D verification: impose `u_x(y)` with Lorentz force balance,
       compare `b_x(y)` to analytic Davidson §3.3 solution.
- [ ] **1.7** Verify low-Rm limit: induced `b` scales linearly with `Ha²`.

**Acceptance**: `force_lorentz { }` on produces coupled momentum+magnetic solution;
Hartmann profile matches analytic; low-Rm scaling confirmed.

### Phase 2 — C1: Full Navier-Stokes + MHD

- [ ] **2.1** Enable `Pb_Hydraulique` coupling: momentum equation solved self-consistently
- [ ] **2.2** Update `.data` file syntax for full coupled cases:
       - `fluide_incompressible` + `Pb_Hydraulique` block + `Convection_Diffusion_Magnetic_field`
- [ ] **2.3** Implement time-stepping that couples `derivee_en_temps_inco` for both equations
- [ ] **2.4** Analytic verification: Shercliff 3D duct experiment (Tassone et al. Fig. 6)
       — compare `u(y), b(y), Ha, Re` profiles
- [ ] **2.5** Parameter scan: `Ha` vs `Re` regime mapping (low-Rm, interaction parameter `M`)

**Acceptance**: Full coupled case runs; `u` and `b` profiles converge to steady state;
Shercliff benchmark data matched within tolerance.

### Phase 3 — C2: Advanced Coupled Features

- [ ] **3.1** Add `∇ · b = 0` enforcement via vector potential `A` (edge elements / Nedelec)
- [ ] **3.2** Implement turbulent MHD: `k-ε` model with MHD modifications (Tassone et al. §4)
- [ ] **3.3** Add species/energy equation with Ohmic heating `J²/σ` source term
- [ ] **3.4** Lid-driven cavity with transverse magnetic field (benchmark case)
- [ ] **3.5** Pump-induced MHD flow in duct (industrial LM application)

**Acceptance**: Advanced features optional; `force_lorentz { }` enables them;
benchmark cases pass.

### Phase 4 — Documentation & Validation

- [ ] **4.1** Update `docs/content/model.md`: mark coupled MHD implemented
- [ ] **4.2** Fill `docs/content/test_cases.md` (coupled MHD cases + how to run)
- [ ] ** ] 4.3 Sanitizer pass: ASan/UBSan on coupled cases
- [ ] **4.4** Review checklist: sign conventions, Lorentz force, div-b, matrix symmetry
- [ ] **4.5** Append to `KERNEL_REUSE_NOTES.md`: coupled MHD sign conventions,
       Ha-Re-M matrix, low-Rm approximation limits

**Acceptance**: all notebooks execute; sanitizers pass; documentation complete.

---

## 4. Key Decisions (settled, new for coupled)

- **D1 — Equation count**: `nombre_d_equations() = 1` in standalone (M0/M1);
  `= 3` when `force_lorentz { }` enabled (momentum + magnetic + time). Further
  extensions (turbulent, energy) add more.
- **D2 — Discretization**: `Polymac_p0` for M0/M1 baseline; `Polymac_p1` or
  edge-elements (`Nedelec`) for coupled when `∇ · b = 0` enforcement needed.
- **D3 — Lorentz force**: `J × B` computed from current `J = curl b / μ₀` using
  same discretization space as `b`. Keyword `force_lorentz { }` off by default.
- **D4 — Velocity coupling**: two pathways:
       - **M0/M1**: `vitesse_magnetique` data field only (no Pb_Hydraulique)
       - **Coupled**: `vitesse_hydraulique` from `Pb_Hydraulique` + MHD source terms
- **D5 — Div-b control**: deferred from M0/M1; added in C2 phase via vector potential
  or constrained transport (edge elements).
- **D6 — Regime**: low-Rm approximation (Rm ≪ 1) for liquid metal breeding blankets
  (Tassone et al.); `(b·∇)u` explicit with lagged `b`; induced field is perturbation.

---

## 5. Risks (coupled MHD specific)

- **Lorentz force sign error** — `J × B` must use consistent sign per Gérard-Varet.
  Wrong sign → unphysical acceleration/deceleration. **P1**: unit tests with analytic
  Hartmann profile before enabling `force_lorentz`.
- **P0 under-resolves `J × B`** — current `J = curl b` needs at least `Polymac_p1` or
  refined mesh for meaningful Lorentz force. **P2**: add edge-element `Nedelec`
  infrastructure for `∇ · b = 0`.
- **Loose coupling instability** — if momentum and magnetic equations are not
  synchronized in time-stepping, may produce spurious oscillations. **P3**: use
  same `Schema_euler_implicit` sub-step for both equations when coupled.
- **Pb_Hydraulique dependency** — enabling coupled mode introduces full hydraulics.
  **P4**: keep `force_lorentz { }` off by default; provide clear migration path.
- **Low-Rm validity** — ensure `(b·∇)u` term uses correct sign and that induced
  field `b` remains perturbation (b ∼ Ha², not O(1)). Test against Tassone et al.
  breeding blanket parameter ranges.
- **Matrix symmetry** — `J × B` term may break symmetry of momentum matrix.
  **P5**: verify `-Werror` clean with `-non-symmetric` flag when Lorentz enabled.

---

## 6. Integration with Existing Infrastructure

### 6.1 Data File Syntax (coupled mode)

```
Read pb
{
  nb_composantes 3        # 1 magnetic + 2 (u,v) or 3 full
  fluide_incompressible
  {
    mu      Champ_Uniforme 1 1.0
    rho     Champ_Uniforme 1 1.0
    lambda  Champ_Uniforme 1 1.0
    Cp      Champ_Uniforme 1 1.0
    beta_th Champ_Uniforme 1 0.0
  }

  # OPTIONAL: hydraulic block (when coupled)
  Pb_Hydraulique ph
  {
    vitesses { ... }
    champs_hydrauliques { ... }
  }

  Convection_Diffusion_Magnetic_field
  {
    nb_composantes         3
    diffusivite_magnetique Champ_Uniforme 1 0.01
    vitesse_magnetique     Champ_Uniforme 3 0.0 0.0 0.0  # data velocity
    # force_lorentz { }   # off by default; on → adds Lorentz force
    diffusion  { }
    convection { amont }
    sources
    {
      induction_stretching { }   # M1 source, optional
    }
  }
}
```

### 6.2 Workflow

- **Standalone (M0/M1)**: `force_lorentz { }` off → `nombre_d_equations() = 1`
  → same as current plan. Velocity from `vitesse_magnetique` only.
- **Coupled**: `force_lorentz { }` on → `nombre_d_equations() = 3`
  → momentum equation activated, `J × B` added, velocity from `Pb_Hydraulique`
  + `vitesse_magnetique` as perturbation.
- **Toggle**: `force_lorentz { }` can be flipped in the `.data` file without
  recompiling code (keyword read at runtime).

### 6.3 Validation Cases (new, planned)

| Case | Reference | Quantities Compared |
|------|-----------|--------------------|
| **Hartmann channel flow** | Davidson §3.3 | `u_x(y), b_x(y), Ha` profile |
| **Shercliff 3D duct** | Shercliff 1953; Tassone et al. | `u(y), b(y), Re, Ha, M` |
| **Lid-driven cavity** | Standard benchmark | `u_x,max, b_z` with transverse B-field |
| **LM breeding blanket** | Tassone et al. 2024 | `Ha, Re, Rm, Ha/√Re` parameter scan |
| **Ponomarenko dynamo** | Gérard-Varet §2 | Kinematic dynamo growth rate |

---

## 7. Differences from Standalone Plan (visual)

```
Standalone (M0/M1)              Coupled MHD
────────────────────────        ────────────────────
b equation only                 b + u equations
force_lorentz off               force_lorentz on
nombre_d_equations() = 1        nombre_d_equations() = 3
vitesse_magnetique only         Pb_Hydraulique + vitesse_magnetique
7 validation .data files          new coupled benchmark cases
low-Rm perturbation               full nonlinear coupling
no div-b enforcement              div-b via edge elements (planned)
```

---

## 8. Migration Path from Standalone

1. **Phase 0–3 of standalone plan** already complete (module compiles, 7 cases run)
2. **Enable `force_lorentz { }`** in a `.data` file → module automatically
   `nombre_d_equations() = 3`
3. **Add `Pb_Hydraulique` block** → full coupled solution
4. **Run coupled benchmark cases** (Hartmann, Shercliff)
5. **Disable `force_lorentz { }`** → revert to standalone M0/M1 behavior
6. **Sanitizer pass** → verify ASan/UBSan on both modes

---

## 9. Timeline (4-thread HDD box)

| Phase | Estimated Time | Notes |
|-------|---------------|-------|
| **0** | 1–2 days | Kernel upgrades, syntax-check, links |
| **1** | 2–3 days | Lorentz force, Hartmann 1D verification |
| **2** | 3–4 days | Full coupled, Shercliff benchmark |
| **3** | 2–3 days | Advanced features (optional) |
| **4** | 1 day | Documentation, review, sanitizers |
| **Total** | **~10–13 days** | Sequential, no full `make optim` |

**Note**: All cases run sequentially (nbProcs=1) per trust-LiMe validation rules.