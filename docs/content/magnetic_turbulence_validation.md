@page TrustLiMe_magnetic_turbulence_validation Magnetic \(k\)-\(\varepsilon\) validation plan

This page records the validation plan for the magnetic \(k\)-\(\varepsilon\) closure in
TRUST-LiMe and the gap between its current status and the verification/validation
level reached by the hydraulic \(k\)-\(\varepsilon\) model in TrioCFD. It is a
working document: items are checked off as they are implemented.

@note The hydraulic \(k\)-\(\varepsilon\) model in TrioCFD is validated at several
levels: manufactured-term verification, analytic-decay checks, physical
validation cases, reference/regression tests, and documented model constants.
The magnetic closure currently has only implementation and smoke coverage.

## Current status

### Hydraulic handoff (Phase 4)
The effective magnetic diffusivity is taken from the hydraulic \(k\)-\(\varepsilon\)
model through `vitesse_hydraulique`:
- `TurbulentDiffusion` exercises the standalone induction equation with a
  turbulent hydraulic momentum equation and checks the non-uniform fold
  \(\eta_{\rm eff} = \eta + \eta_t\).
- `VKSTurbulent` is a 3D coupled MHD integration case; it is a smoke case,
  not a quantitative validation.

### Independent magnetic \(k\)-\(\varepsilon\) (Phase 3)
The source-level closure exists:
`Modele_turbulence_magnetique`,
`Convection_Diffusion_K_Magnetic`,
`Convection_Diffusion_Epsilon_Magnetic`,
`Terme_Source_K_Magnetic`, and
`Terme_Source_Epsilon_Magnetic`.
`MagneticTurbulence` runs end-to-end with finite results, but remains
experimental and is not validated.

### Prescribed \(\eta_t\) (Phases 1--2)
Prescribed uniform or non-uniform turbulent diffusivities are useful for
checking the diffusion path, but they do not validate a turbulence model.

## Proposed validation ladder

### 1. Unit and algebra verification
- Verify \(\eta_t = C_{\mu,b}\,k_b^2 / (\varepsilon_b\,Pm_{t,b})\) and its units.
- Test floors, caps, zero and near-zero states.
- Reject NaN, negative inputs, and incomplete or conflicting providers.
- Test source signs independently:
  - \(k_b\) source: \(P_b - \varepsilon_b\)
  - \(\varepsilon_b\) source:
    \(C_{\varepsilon1,b}(\varepsilon_b/k_b)P_b - C_{\varepsilon2,b}\varepsilon_b^2/k_b\)
- Enforce mutual exclusivity between explicit \(\eta_t\), explicit \(k/\varepsilon\)
  fields, hydraulic fallback, and the independent magnetic model.

### 2. Manufactured-solution verification
Create a TRUST-LiMe analogue of TrioCFD's `Verification_RANS`:
- impose analytic velocity, \(b_A\), \(k_b\), and \(\varepsilon_b\) on a periodic
  domain;
- freeze the solve;
- compare post-processed convective, diffusive, and source terms against an
  independent symbolic reference;
- cover PolyMAC first, then VEF after the port;
- verify \(P_b = \eta_t |\nabla\times b_A|^2\), including zero production for a
  uniform field.

### 3. Transport-only verification
- Set \(P_b = 0\).
- Check advection, diffusion, decay, non-negativity, and mesh/time refinement.
- Add an analytic decay case analogous to TrioCFD's `decroissance_keps`.

### 4. Energy-transfer verification
In a closed domain:
- measure the resolved magnetic energy loss through turbulent diffusion;
- compare it with the integrated production \(P_b\);
- report boundary fluxes separately;
- check that discrepancies are within the expected time and discretization error.

### 5. Coupled regression
- Show that changing \(k_b/\varepsilon_b\) changes \(\eta_{\rm eff}\) and the
  induction decay rate.
- Compare molecular-only, prescribed \(\eta_t\), hydraulic fallback, and the
  independent magnetic model. Pointwise agreement between the two turbulence
  closures is not expected.

### 6. Physical validation
- Promote `MagneticTurbulence` from a smoke case to a real validation form with
  mesh-convergence scales and assertions.
- Add a 3D twin under `MagneticStandalone3D`.
- Use a high-\(\mathrm{Rm}\) or VKS-like case only after calibration, and document
  it as a physical benchmark rather than a smoke test.

## Gap with TrioCFD hydraulic \(k\)-\(\varepsilon\) V&V

1. **Manufactured-term verification**  
   TrioCFD has `Verification_RANS`; TRUST-LiMe has no equivalent for the
   magnetic transport terms.

2. **Analytic-decay validation**  
   TrioCFD has `decroissance_keps`; the magnetic closure has no simple analytic
   reference case.

3. **Physical and experimental validation**  
   TrioCFD validates against channel, backward-step, and diffuser benchmarks.
   TRUST-LiMe currently has smoke and integration cases only.

4. **Unit tests**  
   The required tests for closure, floors, source signs, and provider conflicts
   are not yet in place.

5. **Mesh and time refinement**  
   Validation forms need coarse, standard, and fine scales with assertions, as for
   the established magnetic forms.

6. **Non-regression and reference suite**  
   TrioCFD has many reference cases under `tests/Reference/Turbulence`.
   TRUST-LiMe has no comparable magnetic \(k\)-\(\varepsilon\) reference-test
   inventory.

7. **Discretization coverage**  
   The independent closure is PolyMAC-only and the hydraulic handoff is VEF-only.
   There is no VEF port yet, so no cross-discretization comparison is possible.

8. **Boundary and wall treatment**  
   Only the closed-domain, no-flux scope is realistic today. Inlet, outlet,
   wall-function, and low-Reynolds magnetic corrections are missing.

9. **Restart and state persistence**  
   Save and restore of \(k_b\), \(\varepsilon_b\), and model constants is not yet
   tested.

10. **Coefficient calibration and documentation**  
    Magnetic coefficients currently reuse hydraulic defaults without calibration
    or citations. The equations and validation status also need to be documented.

11. **Positivity and energy balance**  
    Floors, limiters, and energy transfer must be demonstrated numerically, not
    only implemented.

12. **Quantitative coupled feedback**  
    `VKSTurbulent` validates integration, not a quantitative turbulent feedback
    effect.

## Bottom line

To reach TrioCFD-level confidence, the magnetic \(k\)-\(\varepsilon\) closure needs
manufactured verification, analytic-decay checks, unit tests, mesh and time
refinement, a promoted validation form with assertions, cross-discretization
coverage (especially VEF), restart testing, and documented calibration. Until then
it should remain labeled experimental.
