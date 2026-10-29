# Design note — turbulent EDM closure for magnetic fluctuations

Status: experimental design. Builds on `Modele_turbulence_magnetique` (independent
`k_b`/`epsilon_b` closure). No physical validation yet.

## 1. Scope and starting point

The independent closure in `Modele_turbulence_magnetique` already provides

- `eta_t = C_mu_b * k_b^2 / (epsilon_b * Pm_t_b)` (floored/capped),
- `P_b = eta_t * |curl(b_A)|^2`,
- transport of `k_b`, `epsilon_b` as two scalar unknowns,

with frozen PolyMAC_P0 manufactured terms, zero-production analytic decay, a
solved spatial transport case and an energy-transfer case with a molecular twin.
This note sketches the **turbulent electromotive-force (EDM) / β-effect
closure** that makes that machinery usable for VKS-like turbulent sodium:
an effective diffusivity `eta_eff = eta + eta_t` in the mean induction
equation, calibrated and validated against the VKS experimental data in
`share/VKS_experiment_data/`.

The model is isotropic, local, two-equation RANS. It does not resolve `b'` or
`u'`, models no α-effect or magnetic helicity, and does not by itself prove a
self-sustained dynamo. Hydraulic `k`/`epsilon` remain a distinct provider of
`eta_t`.

## 2. Normalization and unit contract

- `b_A = B / sqrt(mu0 * rho_ref)` (Alfvén-normalized field),
- `k_b = 1/2 <|b_A'|^2>` (m²/s²), `epsilon_b` its dissipation (m²/s³),
- requires `C_mu_b*k_b^2/epsilon_b` to have diffusivity units.

First version requires constant `rho_ref` (already enforced in `completer()`)
and a valid `mu0`; variable density and the mapping to the stored (Tesla-valued)
field units are an explicit design gate, not an implicit conversion. Applying
the closure directly to a Tesla-valued field is dimensionally wrong.

## 3. Magnetic-fluctuation transport equations (sketch)

Let `U` be the induction transport velocity, `eta` the molecular magnetic
diffusivity, `b_A` the Alfvén-normalized mean field.

```
∂t k_b + U·∇k_b = ∇·[(eta + eta_t/sigma_kb) ∇k_b] + P_b - epsilon_b

∂t epsilon_b + U·∇epsilon_b =
    ∇·[(eta + eta_t/sigma_epsb) ∇epsilon_b]
    + C_eps1b (epsilon_b/k_b) P_b - C_eps2b epsilon_b^2/k_b

eta_t = min(C_mu_b * k_b^2 / (max(epsilon_b, epsilon_min) * Pm_t_b), eta_t_max)
P_b   = eta_t * |curl(b_A)|^2          (units m²/s³)
```

Floors: `k_b_eff = max(k_b, k_min)`, `epsilon_b_eff = max(epsilon_b, epsilon_min)`
in closure and epsilon source (safeguards, not physical sources; report clip
counts). Negative `k_b` clamps to 0 so `eta_t=0`. The laminar limit is
`eta_t -> 0` as `k_b -> 0`; zero energy must stay well-defined (no 0/0).

Coefficients `C_mu_b, C_eps1b, C_eps2b, sigma_kb, sigma_epsb, Pm_t_b` belong to
the magnetic model, named separately from hydraulic constants, with documented
defaults only after calibration. Do not copy hydraulic k-ε constants as
universal.

The competing stretching production `-b_i b_j ∂_i u_j` is **not** in v1: it
needs anisotropic magnetic-correlation closure and risks double counting. It
may be added later as a separately named, validated option.

## 4. Coupling to the induction equation

- `eta_eff = eta + eta_t`, folded point-wise (`Champ_Fonc_Somme`) or supplied
  as a field.
- Lagged staggered per step: evaluate bounded `eta_t` and cellwise `P_b` from
  the discrete curl of `b_A`; advance `k_b`, `epsilon_b`; refresh `eta_t`;
  solve induction with `eta_eff`; optional `couplage_fort` Picard repeat.
- `eta_t` provider mutual exclusivity: explicit field, explicit `k`/`epsilon`
  pair, hydraulic fallback, and the independent model are at most one
  (enforced by `magnetic_turbulence_provider_configuration_valid`).
- Curl/production must use the same discrete gradient/curl data as the
  induction operator; energy identity holds with curl-form diffusion.

## 5. Calibration targets from VKS experimental data

Sources: `share/VKS_experiment_data/`.

**Sodium properties** (`sodium_properties.json`): ρ≈929 kg/m³, ν≈7.4×10⁻⁷ m²/s,
η≈0.078 m²/s, Pm≈10⁻⁵, σ(T)=108/(6.225+0.0345 T) S/m, T 110–160 °C,
operating up to Rm≈49–52, Re≈5×10⁶.

**Geometry** (`geometry.json`): R_c=0.206 m, impeller radius 0.1545 m,
separation 0.371 m, 8 blades, outer vessel D=0.578 m, V_Na≈0.15 m³;
probe locations P1–P5.

**Dimensionless** (`dimensionless_parameters.json`): Rm=K μ₀ σ 2π R_c² F,
Re=K 2π R_c² F/ν, Θ=(F1−F2)/(F1+F2), Pm=ν/η.

**Low-Rm induction** (`induction_lowRm.json`): magnetic diffusion time 0.15 s
(pulse decay), velocity fluctuation intensity K_u≈0.42, induced-field
fluctuations 20–50 % of mean, spectra ∼ f⁻¹¹/³ (high), ∼ f⁻¹ (mid).

**Fluctuations/spectra** (`fluctuations_spectra.json`): bulk axial ∼ f⁻⁵/³,
azimuthal/radial ∼ f⁻¹, high ∼ f⁻¹¹/³; burst correlation time 2.5 s;
reversal decay 0.8 s⁻¹; PDFs roughly Gaussian.

**Dynamo threshold/saturation** (`dynamo_threshold_saturation.json`): Rm_c
31–43 (config-dependent), Rm_0=35, mean field ≈40 G at 30 % above threshold,
B_sat² ∝ ρ/(μ₀ σ² R_c²)(Rm−Rm_c), power number 0.045 (nondynamo) → 0.065
(dynamo).

**Regimes** (`dynamical_regimes.json`): flow-topology transition Θ≈0.16,
oscillatory threshold Θ_o≈0.41, reversal durations 120/50 s, bursts >100 G.

**Calibration objective**: tune the six magnetic coefficients so that the
predicted `eta_t` yields an effective Rm in the observed threshold range
31–43 and the saturation scaling `B_sat² ∝ (Rm−Rm_c)`. Use Frick et al.
(2010) effective-diffusivity increase (~50 % at Re≈3×10⁶, Rm≈30) as the
physical target for `eta_t` magnitude (no direct `k_b`/`epsilon_b`
measurements exist).

## 6. Validation ladder (status)

Already done:
1. Unit algebra tests (`tests/UnitTests/unit_magnetic_turbulence_kernel.cpp`).
2. Manufactured-term verification (frozen PolyMAC_P0, 8 interior samples).
3. Zero-production analytic decay (3 mesh × 3 dt scan, mesh-independent to 1e-8).
4. Solved spatial transport-only case (P_b=0, nonuniform k_b/epsilon_b).
5. Energy-transfer case with molecular-only twin.

Next steps:
1. Restart persistence of `k_b`, `epsilon_b`, constants; identical continuation.
2. Mesh/time convergence ladder with assertions (coarse/standard/fine).
3. VEF port of `Champ_Fonc_Eta_Turbulent` and `Champ_Fonc_Somme` for
   cross-discretization comparison with the hydraulic handoff path.
4. 3D twin under `MagneticStandalone3D/`.
5. Coupled regression: molecular-only vs prescribed `eta_t` vs hydraulic
   fallback vs independent model; show `eta_eff` and decay-rate change.
6. Physical benchmark: Frick et al. toroidal sodium channel (η_t magnitude);
   Meng/Burr duct for broader MHD turbulence (note: hydraulic k/ε there, not
   magnetic `k_b`).
7. High-Rm/VKS only after calibration; document as benchmark, not smoke.

## 7. Open gates and risks

- No published experiment measures `k_b`/`epsilon_b` directly; calibration is
  indirect via `eta_t` and threshold.
- PolyMAC-only today; VEF port needed for apples-to-apples with hydraulic
  handoff (which is VEF-only).
- Wall functions, low-Re magnetic corrections, inlet/outlet treatments deferred.
- Variable-density mapping gate.
- Stretching production term deferred (energy budget required).
- Keep labeled experimental until the V&V ladder is complete.

## 8. Deliverables

- Update `docs/content/magnetic_turbulence_validation.md` and
  `docs/content/plans/induction_turbulence.md` as steps complete.
- Add calibration coefficients + citations before removing the experimental
  label.
