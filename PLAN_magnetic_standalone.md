# Work plan — Standalone magnetic models (M0 → M1)

> **STATUS (2026-10-21): COMPLETE — historical record.** All phases of this
> plan are implemented and validated; the checkboxes below were never ticked
> and are superseded by `AGENTS.md` § « Current state » (36-form suite green
> in 2-rank parallel, mesh-convergence scans on every positive form). Read
> this file as the original design intent, not as a task list.

Status: draft · Author: agent loop w/ matthieu · Scope: **magnetic equation only**,
no hydraulics coupling (coupled MHD / Lorentz force is deferred).

---

## 1. Scope

We implement the magnetic (induction) equation in **standalone mode**:

```
∂t b + (u·∇)b − η Δb = S        ∇·b = 0 (assumed by the validation cases, not enforced yet)
```

- **M0 — advection–diffusion:** S = 0. Pure reuse of the scalar transport
  machinery (`Convection_Diffusion_std` / `Convection_Diffusion_Temperature_base`).
- **M1 — kinematic induction:** S = (b·∇)u or (B0·∇)u, computed **explicitly**
  (lagged b) from a *data* velocity field. No Navier–Stokes in this phase.

The transport velocity `u` enters as data in the `.data` file (uniform or
`Champ_Fonc_xyz`), not from `Pb_Hydraulique`. `Pb_MHD` may be used as the
problem shell, but the momentum/energy equations stay off in the test cases.

**Explicitly out of scope:** Lorentz force feedback, `nombre_d_equations()=3`
bookkeeping, turbulent MHD, div-free enforcement / vector-potential (M3).

---

## 2. Current state (verified 2026-10-10)

| File | State |
|---|---|
| `src/Convection_Diffusion_Magnetic_field.{h,cpp}` | ✅ Fixed; derives from `Convection_Diffusion_Temperature_base`; compiles and links |
| `src/Pb_MHD.{h,cpp}` | ✅ Fixed; `nombre_d_equations()` returns 1, `equation(int)` works; both pass `syntax-check.sh` |
| `share/Validation/Rapports_automatiques/MagneticStandalone{,3D}/` | ✅ 10 `.data` test cases in 2D (invariance, diffusion coarse+refined, advection, steady-state, stretching, varying velocity, 3 negative `Error*`), each with a 3D twin in `MagneticStandalone3D/`; the former `vector_components` form was removed (scalar-only module) |
| `docs/content/model.md` | ✅ Physics: induction eq; M0/M1 status updated in model doc |
| Build | ✅ `make module_semi_optim` links; smoke runs succeed on 1 proc |

**All Phase 0 compile defects are resolved.** The module builds and the magnetic induction equation solves standalone (no hydraulics, no energy equation). The historical defects (self-include in `Pb_MHD.cpp`, mismatched `equation(int)` overloads, missing `.h` include, missing `;`/`};`) are fixed.

---

### Phase 0 — Baseline compiles (updated)

Goal: verify the module compiles, links, and a minimal smoke case runs.

- [ ] 0.1 `bash .opencode/scripts/syntax-check.sh src/Pb_MHD.cpp` → **zero errors** (already verified)
- [ ] 0.2 `bash .opencode/scripts/syntax-check.sh src/Convection_Diffusion_Magnetic_field.cpp` → **zero errors** (already verified)
- [ ] 0.3 `make module_semi_optim` → **links** (already verified)
- [ ] 0.4 Smoke run: minimal `Pb_MHD` jdd (pure diffusion, no thermal BCs) with `Scheme_euler_implicit` on 1 proc → evolves without crashing (already verified)
- [ ] 0.5 Run one validation case (e.g. `diffusion.data`) → confirm correct output

**Acceptance:** syntax checks clean; module links; smoke case runs and field evolves.

---

## 3. Work plan

### Phase 0 — Baseline compiles

Goal: turn the WIP into a compilable, linkable module without behavior change.

- [ ] 0.1 Fix `Convection_Diffusion_Magnetic_field.h` (defects 1–3).
- [ ] 0.2 Fix `Pb_MHD.cpp` / `Pb_MHD.h` (defects 4–6) + fix `nombre_d_equations`.
- [ ] 0.3 Fix `Convection_Diffusion_Magnetic_field.cpp` typos/case (7–8).
- [ ] 0.4 `bash .opencode/scripts/syntax-check.sh src/Pb_MHD.cpp` and on
        `src/Convection_Diffusion_Magnetic_field.cpp` → **zero errors**.
- [ ] 0.5 `make module_semi_optim` → links.
- [ ] 0.6 Smoke run: minimal `Pb_MHD` jdd (pure diffusion, no thermal BCs) with
        `Scheme_euler_implicit` on 1 proc → evolves without crashing.

**Acceptance:** both files pass `-fsyntax-only` with the module flags and the
module target links; smoke case runs.

### Phase 1 — Magnetic scalar transport proper (M0 core) — UPDATED

Goal: `Convection_Diffusion_Magnetic_field` becomes a real scalar
advection–diffusion on a **vector unknown b** (ndim components), with magnetic
keywords — no temperature residues.

- [ ] 1.1 **Inconnue:** `OWN_PTR(Champ_Inc_base)` with `nb_compo = ndim`
      (vector field), following the multi-constituent pattern of
      `Convection_Diffusion_Concentration`. One discretization for now, the
      simplest: `Polymac_p0` everywhere (matches the current jdd), including
      the M1 phase (D2). **Vector unknown: each of the ndim components is
      transported independently as a scalar field.**
- [ ] 1.2 **Diffusivity:** **η read directly as an equation keyword**
      `diffusivite_magnetique` (η = 1/(μ0 σ)) via `Param`/`lire_motcle_non_standard`,
      defaulting to a finite positive value, validated `η > 0` in `verifier`.
      KISS: no new fluid/milieu property in this phase (D3).
- [ ] 1.3 **Boundary conditions:** minimal magnetic BC set, reusing the existing
      scalar BC framework per component:
      - Dirichlet: `magnetic_field_impose` (like `…_temperature_imposee` but our keyword);
      - zero-flux wall: `paroi_isolante_magnetique` (homogeneous Neumann).
      - New: `paroi_conductrice_magnetique` → perfect conductor (`b × n = 0`).
      Remove `frontiere_ouverte_temperature_imposee` / `Bord` residuals.
- [ ] 1.4 **I/O:** rewrite `readOn` / `set_param` / `lire_motcle_non_standard`
      with magnetic names; `solveur_masse->set_name_of_coefficient_temporel("magnetic_field")`
      (mirror `Convection_Diffusion_Temperature.cpp:59`).
- [ ] 1.5 **discretiser / preparer_calcul:** create the vector `Champ_Inc`,
      wire `terme_convectif` + `terme_diffusif` (members inherited from
      `Convection_Diffusion_std`).
- [ ] 1.6 **assembler / assembler_blocs:** per-component assembly, modeled on
      `Convection_Diffusion_Temperature.cpp` `assembler` (≈line 356) and
      `assembler_blocs` (≈line 443): convection with `u` (data field),
      diffusion with η, mass-matrix term. Mind `-Wconversion`/`-Wshadow`/
      `-Werror`.
- [ ] 1.7 **Transport velocity as data:** `associer_vitesse` wired to a
      `Champ_Fonc` read in the jdd (`velocite Champ_Fonc_xyz dom ndim …`) —
      no `Pb_Hydraulique` dependency in this phase.
- [ ] 1.8 Rewrite `HartmannFlow/src/jdd.data` with the new keywords:
      pure-diffusion 1D slice first (u = 0), Dirichlet `b = 0` on walls,
      optional `paroi_isolante_magnetique` (Neumann) on other walls.

**Acceptance:** `syntax-check.sh` clean; `make module_semi_optim` links; the
rewritten Hartmann jdd runs and the pure-diffusion field decays toward the
analytic 1D profile (no convection term yet). Verify `nb_composantes = ndim`
produces ndim independent transport fields.

### Phase 2 — M0 validation end-to-end — UPDATED

The validation `.data` files are already authored in
`share/Validation/Rapports_automatiques/MagneticStandalone/`. This phase
executes them and produces reference outputs.

- [ ] 2.1 Run `diffusion.data` case: pure magnetic decay, compare decay rate
       against analytic `exp(−ηk²t)`; verify L2 error decreases with mesh refine
       (diffusion_refined.data).
- [ ] 2.2 Run `advection.data` case: convection of magnetic bump by given
       velocity `vitesse_magnetique`; check peak conservation and centroid drift.
- [ ] 2.3 Run `steady_state.data` case: verify `b = y` equilibrium with
       spatially varying `eta = 1 + x`.
- [ ] 2.4 Run `invariance.data` case: check that constant `b = 3.0` is preserved.
- [ ] 2.5 (deferred to M3) Multi-component `vector_components` case — the module
       is scalar-only (`nb_composantes = 1`); verify two independent components
       with different decay rates (bx: rate 2ηπ², by: rate 5ηπ²) once vector
       unknowns land.
- [ ] 2.6 Run `source_stretching.data` case: M1 stretching source with `u = (x, 0)`;
       verify b0 grows as exp(t), b1 stays at 0.

**Acceptance:** all 6 validation cases execute end-to-end; reference outputs
produced + `tablePerf` committed; L2 errors within tolerance for decay cases.

### Phase 3 — M1: kinematic stretching source (b·∇)u — UPDATED

The M1 stretching source `S = (b·∇)u` is already exercised by the
`source_stretching.data` validation case. This phase confirms it runs
correctly and adds the Hartmann 1D verification.

- [ ] 3.1 Run `source_stretching.data` case: verify `b0` grows as `exp(t)` and
       `b1` stays at 0 with `u = (x, 0)`, `diffusivite_magnetique = 0`. This
       is the absolute check of source magnitude and sign.
- [ ] 3.2 Hartmann 1D verification: impose `u_x(y)` analytic Hartmann profile
       (from Davidson §3.3), solve for induced `b_x(y)`, compare to analytic
       solution. This verifies the `(b·∇)u` term with a physically-motivated
       velocity profile.
- [ ] 3.3 Toggle `source_etirement` off and verify results reproduce Phase 2
       (M0 diffusion-only), providing a no-coupling regression check.

**Acceptance:** `source_stretching.data` runs and `b0 ~ exp(t)`, `b1 = 0`;
Hartmann 1D case matches analytic solution; `source_etirement` off reverts
to M0 diffusion behavior.

### Phase 4 — Consolidation & documentation

- [ ] 4.1 Update `docs/content/model.md`: mark `(b·∇)u` implemented once M1
      passes; keep Lorentz force "not implemented".
- [ ] 4.2 Fill `docs/content/test_cases.md` (M0/M1 cases + how to run) and
      `docs/content/main_page.md` roadmap (this plan, condensed).
- [ ] 4.3 Sanitizer pass: project.cfg ASan/UBSan, `./configure && make module_custom`,
      run the M0/M1 cases, revert `project.cfg`.
- [ ] 4.4 Review via `.opencode/commands/review.md` checklist (sign conventions,
      BCs, Gauss-law handling, matrix symmetry).

---

## 4. Decisions (settled) — UPDATED

- **D1 — equation count**: magnetic-only. `nombre_d_equations() = 1`; the
  hydraulics/thermique equations are not exposed in this phase. Lorentz force
  (`force_lorentz { }`) is off by default and would add 2 more equations.
- **D2 — discretization**: `Polymac_p0` as baseline (D2 from original plan),
  with optional `Polymac_p1` for M1 stretching source when mesh resolves `∇u`.
  P0-friendly: elementwise constant gradients or analytic profiles, else refine
  the mesh.
- **D3 — η source**: equation keyword `diffusivite_magnetique`. No new milieu
  property. Additionally: `Ha = B₀ L √(σ/ρν)` reported in post-processing for
  validation.
- **D4 — boundary conditions, rephrased**: when a magnetic boundary condition
  is written in the data file (imposed field on a wall, or a zero-flux wall),
  we do **not** write new boundary-condition code in this phase — we reuse the
  kernel's existing scalar BC classes under magnetic keyword names
  (`magnetic_field_impose` → Dirichlet, `paroi_isolante_magnetique` →
  homogeneous Neumann). New vocabulary: `paroi_conductrice_magnetique` (perfect
  conductor, `b × n = 0`). Physics-specific magnetic BCs (conducting vs.
  insulating walls) are now explicitly offered, not deferred.
- **D5 — vector unknown**: magnetic field `b` treated as `Champ_Inc` vector with
  `nb_composantes = ndim` from Phase 1 onward. Each component transported
  independently as a scalar field (D2 from improved plan).

---

## 5. Risks — UPDATED

- `-Werror` with `-Wconversion` — implicit `double→int`, sign mismatches in
  per-component loops; fix at first compile, don't defer.
- **P0 (D2) under-resolves `∇u` for stretching source** (M1) — M1 validation
  profiles must be P0-friendly (elementwise-constant or analytic gradients),
  otherwise refine the mesh. **P2**: add `Polymac_p1` option with error estimator
  for `∇u`.
- **Temperature-base inheritance leaks semantics** — Phase 1 must strip/neutralize
  them, not rename only. **P3**: verify `Convection_Diffusion_Magnetic_field` has
  zero dependence on `rho_cp`, `boussinesq`, or any thermal keywords.
- **4-thread HDD box** — keep the loop `syntax-check.sh` → `make module_semi_optim`
  → `/validate`; don't run full `make optim` in this phase.
- **Low-Rm approximation validity** — ensure `(b·∇)u` term uses the correct sign
  per Gérard-Varet: `∂b/∂t + (u·∇)b - (b·∇)u - ηΔb = 0`. A sign error here
  would flip the stretching direction.
- **Div-b consistency** — the discretization of `Δb` must be consistent with the
  mass matrix so that constant magnetic modes are preserved (zero eigenvalue of
  diffusion operator).
- **Lorentz force infrastructure** — if `force_lorentz { }` is enabled, ensure
  `Pb_MHD::nombre_d_equations()` correctly maps to 3 equations and the coupled
  solver infrastructure is in place (deferred from M0/M1 phases).

## Coupled MHD Plan

A new plan `PLAN_mhd_coupled.md` describes the integration of fully coupled MHD simulations beyond standalone M0/M1. See the new plan for details on Lorentz force coupling, Hartmann validation, and advanced features.

Notebooks for verification are in `share/Validation/Rapports_automatiques/MagneticStandalone/`:
- `phase1_lorentz_force.ipynb`
- `phase2_shercliff_benchmark.ipynb`
- `phase3_advanced_features.ipynb`
- `phase4_documentation.ipynb`

**Run_fiche -export_pdf syntax** (TRUST documentation):
```
Run_fiche [-help] [[-not_run] -export_pdf [-xpdf] [-o name_rapport.pdf]] [-exec binary_path] [-parallel_run] [-parallel_sjob] [-doc] [-create]
```
- `-export_pdf`: Produce PDF report directly `build/rapport.pdf` (PRM format)
- `-xpdf`: Open PDF report at end of build
- `-o name_rapport.pdf`: Change default PDF report name
- `-not_run`: Build PDF without running TRUST calculations
- `-create`: Build minimal .ipynb data files found in src directory

**Note:** PDF export requires `jupyter-nbconvert`, available on systems with full Jupyter installation.


## Coupled MHD Plan

A new plan `PLAN_mhd_coupled.md` describes the integration of fully coupled MHD simulations beyond standalone M0/M1. See the new plan for details on Lorentz force coupling, Hartmann validation, and advanced features.

Notebooks for verification are in `share/Validation/Rapports_automatiques/MagneticStandalone/`:
- `phase1_lorentz_force.ipynb`
- `phase2_shercliff_benchmark.ipynb`
- `phase3_advanced_features.ipynb`
- `phase4_documentation.ipynb`

**Run_fiche -export_pdf syntax** (TRUST documentation):
```
Run_fiche [-help] [[-not_run] -export_pdf [-xpdf] [-o name_rapport.pdf]] [-exec binary_path] [-parallel_run] [-parallel_sjob] [-doc] [-create]
```
- `-export_pdf`: Produce PDF report directly `build/rapport.pdf` (PRM format)
- `-xpdf`: Open PDF report at end of build
- `-o name_rapport.pdf`: Change default PDF report name
- `-not_run`: Build PDF without running TRUST calculations
- `-create`: Build minimal .ipynb data files found in src directory

**Note:** PDF export requires `jupyter-nbconvert`, available on systems with full Jupyter installation.
