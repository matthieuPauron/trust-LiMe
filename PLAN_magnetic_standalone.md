# Work plan — Standalone magnetic models (M0 → M1)

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
| `src/Convection_Diffusion_Magnetic_field.{h,cpp}` | WIP, derives from `Convection_Diffusion_Temperature_base`; **does not compile** |
| `src/Pb_MHD.{h,cpp}` | Problem shell; `nombre_d_equations()` returns 1 (wrong), `equation(int)` broken |
| `share/Validation/…/HartmannFlow/src/jdd.data` | Uses `Pb_MHD` but magnetic block still reads **thermal keywords** (`Temperature`, `frontiere_ouverte_temperature_imposee`) |
| `share/Validation/…/Shercliff_3D_duct/src/jdd.data` | **Empty (0 bytes)** |
| `docs/content/model.md` | Physics: induction eq + both source terms marked "not implemented yet" |

### Concrete compile defects to fix (Phase 0)

`src/Convection_Diffusion_Magnetic_field.h`
1. Line 4: `#include <Convection_Diffusion_Temperature>` — wrong (no `.h`, and we
   derive from `Convection_Diffusion_Temperature_base`): include
   `<Convection_Diffusion_Temperature_base.h>`.
2. Line 17: `Champ_Inc_base& inconnue() override { return the_magnetic_field_ } ;`
   — missing `;` **inside** the body (`…; }`) before `}`; keep the implicit
   `OWN_PTR → ref` conversion used by the temperature base (`return la_temperature;`).
3. Line 29: class ends with `}` instead of `};`.

`src/Pb_MHD.cpp`
4. Line 8: `#include <Pb_MHD.cpp>` — self-include; replace with `#include <Pb_MHD.h>`.
5. `equation(int)` defined twice **with different return types** (`const …&`
   line 18-29 i.e. re-declared as `Equation_base& … const`, lines 31-42) while the
   header's non-const `Equation_base& equation(int)` override is never defined.
   Keep exactly the two header signatures (const + non-const), correct index map
   0→hydraulique, 1→thermique, 2→magnetic, and `Process::exit()` on OOB.
6. `Pb_MHD::nombre_d_equations()` — keep `1`: **magnetic-only problem** (D1).
   The `equation(int)` accessor still maps 0→hydraulique, 1→thermique,
   2→magnetic as scaffolding, but `nombre_d_equations() == 1` means only the
   magnetic equation is exposed to the driver in this phase.

`src/Convection_Diffusion_Magnetic_field.cpp`
7. `Convection_diffusion_Temperature_base::readOn` → correct case
   (`Convection_Diffusion_Temperature_base`).
8. `Mocle` / `Motclef` typos → `Motcle`.
9. `printOn/readOn`, `set_param`, `lire_motcle_non_standard` are pass-throughs to
   the temperature base — acceptable for Phase 0, replaced in Phase 1.

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

### Phase 1 — Magnetic scalar transport proper (M0 core)

Goal: `Convection_Diffusion_Magnetic_field` becomes a real scalar
advection–diffusion on a **vector unknown b**, with magnetic keywords —
no temperature residues.

- [ ] 1.1 **Inconnue:** single `OWN_PTR(Champ_Inc_base)` with `nb_compo = ndim`
      (vector field), following the multi-constituent pattern of
      `Convection_Diffusion_Concentration`. **One discretization for now, the
      simplest: `Polymac_p0`** everywhere (matches the current jdd), including
      the M1 phase (D2).
- [ ] 1.2 **Diffusivity:** **η read directly as an equation keyword**
      `diffusivite_magnetique` (η = 1/(μ0 σ)) via `Param`/`lire_motcle_non_standard`,
      defaulting to a finite positive value, validated `η > 0` in `verifier`.
      KISS: no new fluid/milieu property in this phase (D3).
- [ ] 1.3 **Boundary conditions:** minimal magnetic BC set, reusing the existing
      scalar BC framework per component:
      - Dirichlet: `magnetic_field_impose` (like `…_temperature_imposee` but our keyword);
      - zero-flux wall: `paroi_isolante_magnetique` (homogeneous Neumann).
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
      pure-diffusion 1D slice first (u = 0), Dirichlet b = 0 on walls.

**Acceptance:** `syntax-check.sh` clean; `make module_semi_optim` links; the
rewritten Hartmann jdd runs and the pure-diffusion field decays toward the
analytic 1D profile (no convection term yet).

### Phase 2 — M0 validation end-to-end

- [ ] 2.1 Extend `HartmannFlow.ipynb`: analytic cells (decay / profile),
      L2-error cell, plot b; run with `trustutils` (nbProcs=1).
- [ ] 2.2 Create `Shercliff_3D_duct/src/jdd.data` (3D, P0, u=0 → isotropic
      diffusion) and its notebook cells.
- [ ] 2.3 Execute both notebooks via the `/validate` recipe; commit reference
      outputs + `tablePerf`.

**Acceptance:** both notebooks execute end-to-end; L2 error below tolerance;
results reproducible on this machine.

### Phase 3 — M1: kinematic stretching source (b·∇)u

- [ ] 3.1 Implement the source `S = (b·∇)u`, **explicit** (lagged b):
      either a `Source_base` subclass (pattern: `Terme_Source_Qdm`)
      or a direct second-member contribution in `derivee_en_temps_inco`.
      Keyword `source_etirement { }` to enable (off ⇒ M0 behavior).
- [ ] 3.2 `∇u` for the data velocity: analytic gradient fields supplied in the
      jdd (valid for the polynomial/Hartmann profiles), or a per-face numeric
      gradient if available — keep out of the coupled-NS path.
- [ ] 3.3 Hartmann 1D verification: impose `ux(y)` analytic Hartmann profile,
      solve for induced `bx(y)`, compare to the analytic solution.

**Acceptance:** notebook shows induced-field profile matching analytics within
tolerance; toggling `source_etirement` off reproduces Phase 2 results
(no-coupling regression check).

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

## 4. Decisions (settled)

- **D1 — equation count:** magnetic-only. `nombre_d_equations() = 1`; the
  hydraulics/thermique equations are not exposed in this phase.
- **D2 — discretization:** one, the simplest: `Polymac_p0`, used uniformly for
  M0 and M1 (the stretching source must then be P0-friendly: elementwise
  constant gradients / analytic profiles, or refined meshes).
- **D3 — η source:** equation keyword `diffusivite_magnetique`. No new milieu
  property.
- **D4 — boundary conditions, rephrased:** when a magnetic boundary condition
  is written in the data file (imposed field on a wall, or a zero-flux wall),
  we do **not** write new boundary-condition code in this phase — we reuse the
  kernel's existing scalar BC classes under magnetic keyword names
  (`magnetic_field_impose` → Dirichlet, `paroi_isolante_magnetique` →
  homogeneous Neumann). Physics-specific magnetic BCs (conducting vs.
  insulating walls) are deferred; the vocabulary is magnetic now, the machinery
  is the kernel's.

---

## 5. Risks

- `-Werror` with `-Wconversion` — implicit `double→int`, sign mismatches in
  per-component loops; fix at first compile, don't defer.
- P0 (D2) under-resolves `∇u` for the stretching source; M1 validation
  profiles must be P0-friendly (elementwise-constant or analytic gradients),
  otherwise refine the mesh.
- The temperature-base inheritance leaks temperature semantics (`rho_cp`,
  `boussinesq`) — Phase 1 must strip or neutralize them, not rename only.
- 4-thread HDD box: keep the loop `syntax-check.sh` → `make module_semi_optim`
  → `/validate`; don't run full `make optim` in this phase.