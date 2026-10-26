@page TrustLiMe_magnetic_turbulence_validation Magnetic k-epsilon validation plan

This page records the validation plan for the magnetic \f$k\f$-\f$\varepsilon\f$ closure in
TRUST-LiMe and the gap between its current status and the verification/validation
level reached by the hydraulic \f$k\f$-\f$\varepsilon\f$ model in TrioCFD. It is a
working document: items are checked off as they are implemented.

@note The hydraulic \f$k\f$-\f$\varepsilon\f$ model in TrioCFD is validated at several
levels: manufactured-term verification, analytic-decay checks, physical
validation cases, reference/regression tests, and documented model constants.
The independent magnetic closure now has algebra unit tests, a PolyMAC
manufactured-term verification, and a zero-production analytic-decay check,
but no physical validation.

## Current status

### Hydraulic handoff (Phase 4)
The effective magnetic diffusivity is taken from the hydraulic \f$k\f$-\f$\varepsilon\f$
model through `vitesse_hydraulique`:
- `TurbulentDiffusion` exercises the standalone induction equation with a
  turbulent hydraulic momentum equation and checks the non-uniform fold
  \f$\eta_{\rm eff} = \eta + \eta_t\f$.
- `VKSTurbulent` is a 3D coupled MHD integration case; it is a smoke case,
  not a quantitative validation.

### Independent magnetic \f$k\f$-\f$\varepsilon\f$ (Phase 3)
The source-level closure exists:
`Modele_turbulence_magnetique`,
`Convection_Diffusion_K_Magnetic`,
`Convection_Diffusion_Epsilon_Magnetic`,
`Terme_Source_K_Magnetic`, and
`Terme_Source_Epsilon_Magnetic`.
`MagneticTurbulence` now verifies frozen manufactured convective, diffusive,
and source terms, plus zero-production time decay, on PolyMAC_P0. It is
verification coverage only; the closure remains experimental and has no
physical validation.

### Manufactured-term verification (implemented 2026-10-25)
The `MagneticStandalone/MagneticTurbulence/` form prescribes analytic
divergence-free velocity, Alfvén-normalized \f$b_A\f$, \f$k_b\f$, and
\f$\varepsilon_b\f$, then freezes all equations with zero time steps. It
post-processes convection, diffusion, and source terms for both transport
equations and compares eight interior samples with independently differentiated
analytic expressions, including variable \f$\eta_t\f$ in the diffusion terms
and \f$P_b=\eta_t|\nabla\times b_A|^2\f$ in the sources.

PolyMAC_MPFA does not implement periodicity, so this first case uses a closed
unit square with homogeneous no-flux walls. The cosine fields have zero normal
derivatives at those walls. On the 65×65-node mesh, maximum relative errors
were 0.78%/0.13%/0.53% for k convection/diffusion/source and
0.78%/0.13%/0.38% for epsilon, below the notebook limits of 12% for
convection/diffusion and 3% for sources. This does not replace mesh refinement
or a VEF port.

### Zero-production analytic decay (implemented 2026-10-25)
The same form adds a uniform-field, zero-velocity case, so \f$P_b=0\f$ and
convective/diffusive transport vanish. It checks the coupled source ODEs
\f$dk_b/dt=-\varepsilon_b\f$ and
\f$d\varepsilon_b/dt=-C_{\varepsilon2,b}\varepsilon_b^2/k_b\f$ against their
closed-form solution. Three mesh levels (17, 33, 65 nodes) are crossed with
three binary-exact time steps (0.125, 0.0625, 0.03125) to \f$t=2\f$. Both
unknowns remain positive, temporal errors decrease with each timestep
halving, and final values are mesh-independent to \f$10^{-8}\f$. Coarse-step
relative errors are 0.113% for \f$k_b\f$ and 0.612% for \f$\varepsilon_b\f$;
at the finest step they are 0.028% and 0.150%. The run also asserts zero
post-processed convection and diffusion (below \f$10^{-12}\f$).

### Prescribed \f$\eta_t\f$ (Phases 1--2)
Prescribed uniform or non-uniform turbulent diffusivities are useful for
checking the diffusion path, but they do not validate a turbulence model.

## Validation ladder

### 1. Unit and algebra verification
- Core tests for the diffusivity formula, floors/cap, production scaling, source
  formulas, and incomplete/conflicting providers are implemented in
  `tests/UnitTests/unit_magnetic_turbulence_kernel.cpp`.
- Verify \f$\eta_t = C_{\mu,b}\,k_b^2 / (\varepsilon_b\,Pm_{t,b})\f$ and its units.
- Test floors, caps, zero and near-zero states.
- Define and test NaN/negative input behavior (clamp versus reject); reject
  incomplete or conflicting providers.
- Test source signs independently:
  - \f$k_b\f$ source: \f$P_b - \varepsilon_b\f$
  - \f$\varepsilon_b\f$ source:
    \f$C_{\varepsilon1,b}(\varepsilon_b/k_b)P_b - C_{\varepsilon2,b}\varepsilon_b^2/k_b\f$
- Enforce mutual exclusivity between explicit \f$\eta_t\f$, explicit \f$k/\varepsilon\f$
  fields, hydraulic fallback, and the independent magnetic model.

### 2. Manufactured-solution verification
Create a TRUST-LiMe analogue of TrioCFD's `Verification_RANS`:
- impose analytic velocity, \f$b_A\f$, \f$k_b\f$, and \f$\varepsilon_b\f$ on a closed
  domain with analytic no-flux boundary compatibility (PolyMAC_MPFA currently
  does not support periodic boundaries);
- freeze the solve;
- compare post-processed convective, diffusive, and source terms against an
  independent analytic reference;
- cover PolyMAC first, then VEF after the port;
- verify \f$P_b = \eta_t |\nabla\times b_A|^2\f$, including zero production for a
  uniform field.

Status: the frozen PolyMAC_P0 term comparison is implemented in
`MagneticTurbulence`; VEF and periodic-boundary coverage remain open.

### 3. Transport-only verification
- The uniform-field analytic-decay scan now sets \f$P_b=0\f$ and checks
  source-driven decay, zero advection/diffusion, positivity, timestep
  convergence, and mesh independence.
- Advection/diffusion are covered by the frozen manufactured terms; a solved
  spatially varying transport-only case remains useful follow-up coverage.

### 4. Energy-transfer verification
In a closed domain:
- measure the resolved magnetic energy loss through turbulent diffusion;
- compare it with the integrated production \f$P_b\f$;
- report boundary fluxes separately;
- check that discrepancies are within the expected time and discretization error.

### 5. Coupled regression
- Show that changing \f$k_b/\varepsilon_b\f$ changes \f$\eta_{\rm eff}\f$ and the
  induction decay rate.
- Compare molecular-only, prescribed \f$\eta_t\f$, hydraulic fallback, and the
  independent magnetic model. Pointwise agreement between the two turbulence
  closures is not expected.

### 6. Physical validation
- Extend the current verification form with mesh-convergence scales and
  assertions before treating it as a physical validation form.
- Add a 3D twin under `MagneticStandalone3D`.
- Use a high-\f$\mathrm{Rm}\f$ or VKS-like case only after calibration, and document
  it as a physical benchmark rather than a smoke test.

## Gap with TrioCFD hydraulic \f$k\f$-\f$\varepsilon\f$ V&V

1. **Manufactured-term verification**  
   A frozen PolyMAC_P0 analogue now checks convective, diffusive, and source
   terms. It uses no-flux boundaries because periodicity is not implemented in
   PolyMAC_MPFA; VEF coverage remains open.

2. **Analytic-decay validation**  
   A zero-production coupled-source ODE analogue now has a closed-form
   reference and a three-level time scan; solved spatial transport-only decay
   remains untested.

3. **Physical and experimental validation**  
   TrioCFD validates against channel, backward-step, and diffuser benchmarks.
    TRUST-LiMe now has a manufactured-term verification plus smoke/integration
    cases, but no physical or experimental validation.

4. **Unit tests**  
   Core tests for closure algebra, floors/caps, source signs, and provider
   conflicts are in place; broader invalid-input and integration coverage
   remains useful.

5. **Mesh and time refinement**  
   Validation forms need coarse, standard, and fine scales with assertions, as for
   the established magnetic forms.

6. **Non-regression and reference suite**  
   TrioCFD has many reference cases under `tests/Reference/Turbulence`.
   TRUST-LiMe has no comparable magnetic \f$k\f$-\f$\varepsilon\f$ reference-test
   inventory.

7. **Discretization coverage**  
   The independent closure is PolyMAC-only and the hydraulic handoff is VEF-only.
   There is no VEF port yet, so no cross-discretization comparison is possible.

8. **Boundary and wall treatment**  
   Only the closed-domain, no-flux scope is realistic today. Inlet, outlet,
   wall-function, and low-Reynolds magnetic corrections are missing.

9. **Restart and state persistence**  
   Save and restore of \f$k_b\f$, \f$\varepsilon_b\f$, and model constants is not yet
   tested.

10. **Coefficient calibration and documentation**  
     Magnetic coefficients still reuse hydraulic defaults without calibration
     or citations; the equations and validation status are now documented.

11. **Positivity and energy balance**  
    Floors, limiters, and energy transfer must be demonstrated numerically, not
    only implemented.

12. **Quantitative coupled feedback**  
    `VKSTurbulent` validates integration, not a quantitative turbulent feedback
    effect.

## Bottom line

Manufactured-term and zero-production analytic-decay verification, including
core algebra tests and an initial mesh/time scan, are now in place. To reach
TrioCFD-level confidence, the magnetic \f$k\f$-\f$\varepsilon\f$ closure still
needs solved spatial transport-only coverage, broader refinement, VEF coverage,
restart testing, and documented calibration.
Until then it should remain labeled experimental.
