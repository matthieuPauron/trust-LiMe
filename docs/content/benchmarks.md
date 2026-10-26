@page TrustLiMe_benchmarks Benchmarks

This page summarises the TRUST-LiMe validation benchmarks: what is tested, the
analytic reference, the tolerance, and the mesh-convergence behaviour.
See @ref TrustLiMe_validation "Validation cases" for the per-form directory
layout and @ref TrustLiMe_testcases "Test cases" for the unit-test layer.

## Suite overview

The validation suite covers 27 forms across five categories:

| Category | Forms | What it exercises |
|---|---|---|
| `MagneticStandalone` (shared 2D/3D forms) | 15 | Standalone induction equation: paired dimension cases plus MMS_2D, Shercliff2D, turbulence, and error inputs |
| `MagneticStandalone3D` (3D-only forms) | 2 | `MMS_3D` (four historical cases) and `Shercliff3D` (full vector model) |
| `MagneticCoupled` (2D) | 6 | Coupled low-Rm MHD: ForceLorentzBalance, MMS_Coupled (with 3D dataset), Coupled_Feedback, HartmannTwoWay (+ Ha scan), ErrorInputs |
| `MagneticCoupled3D` | 2 | Coupled 3D: VKSLite (smoke), ShercliffDuct3D (coupled duct) |
| `Postraitement_fields` | 2 | Post-treatment fields: `Champs2D`, `Champs3D` (merged forms) |

Every positive form carries a **mesh-convergence scan** on a shared
`coarse` / `standard` / `fine` ladder (cells ×2 then ×4 in the active
directions), run from a single substituted `src/<case>.data` template.

The experimental `MagneticStandalone/MagneticTurbulence/` form exercises the
independent magnetic k-\f$\varepsilon\f$ closure with frozen manufactured-term
checks and a zero-production analytic-decay scan on PolyMAC_P0. It is not yet
a physical validation benchmark and has no VEF twin.

## Key benchmarks

### Shercliff2D / Shercliff3D — parabolic-shear channel

- **Analytic reference:** `b_x(y) = (B_0 U_0/η)(y³/(3h²) − y)`,
  `J_z(y) = (B_0 U_0/(η μ_0))(1 − (y/h)²)`, `b_y ≡ B_0`.
- **What it catches:** inductive stretching with a no-slip parabolic profile,
  the `courant_magnetique` post-treatment, insulating walls, exact invariant
  `b_y = B_0`.
- **Tolerance:** core probes < 5 % (coarse), wall probes < 35 % (documented
  O(h) wall-cell gradient bias), `J_z` < 5 %.
- **Convergence:** first order (≈ 1.08, 0.96 observed) — limited by the
  wall-cell gradient bias, which halves on refinement.
- **3D twin** adds exact extrusion invariants: `b_z ≡ 0`, z-uniformity,
  vector `courant_magnetique`.

### HartmannTwoWay — driven Hartmann flow (two-way coupling)

- **Analytic reference:** Hartmann profile at Ha = 5 (12.7× below Poiseuille),
  stretching-induced `b_x`, magnetic-pressure profile `p(y) = P − b_x²/(2μ₀)`.
- **What it catches:** both feedback directions live, open pressure boundaries,
  suppression factor `σ(Ha) = Ha²/(2(1−1/cosh Ha))` within 1.5 % at
  Ha ∈ {2.5, 5, 7.9}. The complete 12-point grid also checks
  `Re_H = ρ U_H(0)L/μ ∈ {0.01, 0.1, 1, 10}` against every Ha level at
  fixed `Rm = 0.1`, including both dimensionless numbers, velocity,
  induced-field and invariant checks.
- **Scope limit:** the fully developed profile is unidirectional, so
  `u·∇u = 0`; its normalized solution has no inertial Re dependence. This grid
  validates parameter scaling for the exact Hartmann solution, not an inertial
  transition or a general `Ha`–`Re` regime map. A non-fully-developed duct
  benchmark is needed for that. An exploratory `Re_H=100, Ha=7.9` case left
  the Hartmann branch in the finite-length duct and is outside the checked map.

### SourceStretching — M1 induction source

- **Analytic reference:** `b(t) = B_0 e^t` with `u = (x, −y)` (div-free).
- **What it catches:** stretching source magnitude and sign; a wrong factor,
  transposed gradient index, or dropped term changes the exponential rate.

### MMS (MMS_2D / MMS_3D)

- **Analytic reference:** manufactured solutions covering diffusion, trigonometric,
  exponential-trigonometric, and polynomial profiles.
- **What it catches:** spatial convergence order of advection + diffusion +
  stretching; the `MMS_Diff2D`/`MMS_Trig2D` pin the invariance of the uniform
  (0, 1) field; `MMS_Diff3D` needed mesh refinement (5³ → 21³) for the P0
  projection.

### Hartmann1D — Hartmann induced field (1D physics)

- **Analytic reference:** Hartmann profile.
- **What it catches:** documented wall-cell gradient bias (measured exactly
  halved in the two wall-adjacent cells); `nb_composantes 2`.

## Mesh-convergence convention

Every positive form runs a `coarse` / `standard` / `fine` scan from a single
template `src/<case>.data` with `$nx/$ny[$nz]` placeholders, substituted via
`run.addCaseFromTemplate` into `build/<scale>/`. Error forms (`ErrorInputs`,
merged per category) are excluded by design.

## Known limitations

- **Wall-cell gradient bias:** the M1 source needs `∇u`; the kernel builds it
  by averaging face gradients, and a boundary face only gets a Dirichlet
  contribution. The two cells adjacent to a wall receive a biased `∇u`
  (measured exactly halved for a linear velocity). Core probes are asserted
  tightly; wall probes only against a bound.
- **`∇·b` not enforced:** assumed but not proven globally (same as the
  thermalhydraulics velocity field). Probes must target interior cells.
- **Low-Rm approximation:** validated against Tassone et al. breeding-blanket
  ranges.
- **Two-way coupling is lagged:** staggered scheme `bˢ⁺¹ = F(bˢ, uˢ)`,
  `uˢ⁺¹ = G(uˢ, bˢ)` — linearizes both Lorentz and stretching terms.
- **No turbulent MHD models:** EDM/VKS deferred.

## Open items

- **Inertial Re dependence** in the Ha/Re regime map: the HartmannTwoWay
  parameter grid is complete for its exact solution, but a geometry with
  nonzero convective acceleration is still needed to validate Re effects.
- **Shercliff 3D duct** (Tassone et al. Fig. 6) — coupled, blocked by §18.2
  heisenbug for new 3D forms.
- **`paroi_conductrice_magnetique`** (Phase B.2) — strict `b·n = 0` and
  `b×n = 0`, needs per-component CL class.
- **3D twins** for `HartmannTwoWay`, `ForceLorentzBalance`, `Coupled_Feedback`.
- **div-b enforcement** (Path 2, pressure-correction approach).
- **Turbulent EDM/VKS** (docs/content/end_goal.md Phase 3).
- **§18.2 kernel fix** (upstream code audit of `Op_Diff_PolyMAC_MPFA_Elem`).

## Unit tests

42 tests from 9 suites in `tests/UnitTests/` (GoogleTest, kernel conventions).
Fast path: `--arith-only` (25 tests, micro + numeric kernels). Full suite
requires a kernel build. Includes `Terme_Source_Lorentz` data-file contract
(`unit_lorentz_source.cpp`).
EOF
echo "written"
