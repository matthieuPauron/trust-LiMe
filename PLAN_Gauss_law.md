# Plan — Gauss' law enforcement (Path 2: opt-in divergence correction)

**Status: implementation and first validation in progress (2026-10-24).** §6.2
items 1–4 (projection kernel, equation/Picard hooks, and arith-only unit tests)
are in the tree; `make module_semi_optim` builds cleanly. The 2D `GaussLaw`
form is present, but its two-rank run exposed a ghost-potential bug that must be
fixed and revalidated. Remaining: the 3D twin, `ErrorInputs` additions, full
validation, and docs. Reserves the M3 milestone
(Gauss' law enforcement) already earmarked in `docs/content/model.md` and
`KERNEL_REUSE_NOTES.md` §11bis.

Built on the full picture from both sides — the module's ∇·b state and the
kernel's incompressible machinery — the sections below give the design
rationale, then the concrete implementation plan.

---

## 1. What "enforcing Gauss' law" means here

- `b` is cell-centred (`dis.concentration(...)` machinery, `Polymac_p0` in
  nearly all forms, 2 `VEFPreP1B` cases). Its reported divergence — the one
  feeding `tension_magnetique`, `force_lorentz`, and the
  `divergence_magnetique` probe — is `tr(∇b)` from Green–Gauss with centred
  face averages (`gradient_b_elements`, Pass 1 + B2).
- Today that divergence is assumed zero (Path 1, `KERNEL_REUSE_NOTES.md`
  §11bis); the notes and `docs/content/model.md` already reserve the M3
  milestone for enforcement.

## 2. What TRUST does for ∇·u = 0 (the elements worth taking)

The kernel's answer is a Hodge projection, and it lives in the equation, not
in the CLs:

- **The projection itself** — `Navier_Stokes_std::projeter()`
  (`src/ThHyd/Incompressible/Equations/Navier_Stokes_std.cpp:817`):

  ```
  divergence.calculer(u, secmem)
    → solveur_pression_.resoudre_systeme(matrice_pression_, secmem, lagrange)
    → assembleur_pression_->modifier_solution(lagrange)
    → gradient->multvect(lagrange, gradP)
    → u -= dt·M⁻¹Bᵀλ
    → solveur_masse->corriger_solution(u, u)   # re-impose Dirichlet
    → recompute divergence and print the "bilan avant/après"
  ```

- **Singularity handling:** `Assembleur_P_PolyMAC_HFV::modifier_solution()`
  just pins the constant (`pression -= mp_min_vect(pression)`); the PolyMAC
  assembler penalises element 0 when no pressure-Dirichlet exists
  (`mat(0,0) *= 2`).
- **Hooks:** an initial projection in `preparer_calcul()`, per-step updates
  via `Probleme_base::mettre_a_jour(temps)` →
  `equation(i).mettre_a_jour(temps)` (runs before probes/post-treatment, so
  sondes see the cleaned field), and a divergence watchdog (`seuil_divU`).
- **Reusable building blocks:** `SolveurSys`, `Matrice_Morse`, `Stencil` +
  `Matrix_tools::allocate_morse_matrix` (what
  `Assembleur_P_PolyMAC_MPFA::assembler_mat` uses), `mp_min_vect` /
  `mp_norme_vect`.

**Not reusable verbatim:** `Assembleur_P_*` and `Operateur_Div` / `G_Grad` —
they are built for the face-centred velocity unknown (`B·u` from face
fluxes, `L = B M⁻¹Bᵀ`). `b` is cell-centred, so we must build the analogous
triplet ourselves.

## 3. Proposed design — `correction_divergence { }` (opt-in Path 2)

A per-step magnetic Hodge projection owned by
`Convection_Diffusion_Magnetic_field`, mirroring `projeter()` step for step:

1. **Consistent operator triplet (the core requirement).** Build
   module-side, in a new `src/Correction_Divergence.{h,cpp}`:
   - `D` = exactly the Pass-1 Green–Gauss divergence (centred averages
     interior, element value at wall faces) — the same operator that defines
     the reported `divergence_magnetique`;
   - `G` = its adjoint-shaped gradient of the scalar `φ`;
   - `L = D∘G` assembled once as a `Matrice_Morse` (face-graph stencil,
     `Matrix_tools::allocate_morse_matrix`).

   Solving `Lφ = D b*` then correcting `b ← b − Gφ` gives
   `D b_new = D b* − Lφ` = solver residual ≈ 1e-12 — machine-precision
   Gauss' law in the module's own divergence measure (and the B2 boundary
   report borrows interior gradients, so wall cells report ≈ 0 too).
   Reusing e.g. the MPFA diffusion operator as `L` instead would only
   *reduce* divergence, not annihilate it — that's why the triplet must be
   self-built and self-consistent.

2. **Boundary conditions.** At `magnetic_field_impose` (Dirichlet) faces:
   Neumann data `∂φ/∂n = (b* − b_cl)·n` into the secmem (the
   pressure-secmem trick), then re-impose Dirichlet element values after
   correction via `solveur_masse->corriger_solution(b, b)` — the same call
   NS uses on velocity. At Neumann walls the zero-normal-gradient convention
   is already part of `D`.

3. **Compatibility = Gauss' law as an input contract.** A pure-Neumann `φ`
   problem is solvable iff `∮ b_cl·n dS = 0`. Net prescribed magnetic flux
   ≠ 0 is a genuine Gauss-law violation, so: pin the constant
   (`φ -= mp_min_vect(φ)`, the `modifier_solution` pattern) and
   `Process::exit` with a clear message when the net flux is non-zero — a
   physics check NS never needs but we do.

4. **Hooks (three):**
   - `mettre_a_jour(temps)` override → clean `inconnue().valeurs()` after
     each completed step. Works unchanged for `Pb_Induction`, `Pb_MHD`,
     `Pb_MHD_Turbulent`, `Pb_MHD_Thermal`; idempotent by construction
     (already-solenoidal ⇒ `φ ≈ 0` ⇒ `b` untouched — which is what keeps the
     green baseline byte-identical when the keyword is absent, and makes the
     invariance test possible).
   - `preparer_calcul()` → initial cleaning of a non-solenoidal `Champ_Init`
     (the analog of NS's "Projection of initial and boundaries conditions").
   - Picard mode: `mhd_iterate_time_step` (`MHD_Coupling.cpp`) must clean
     `futur()` explicitly per sweep — in `couplage_fort` the new solution
     lives in `futur()`, not `valeurs()`, so `mettre_a_jour` alone would
     clean the wrong slot. Module code owns that loop, so this is a 2-line
     call plus convergence measured on solenoidal iterates.

5. **Data surface.** Block keyword
   `correction_divergence { solveur_divergence ... frequence_correction n seuil_divergence s }`.
   `frequence` default 1, `seuil_divergence` = post-correction watchdog
   abort (mirrors `seuil_divU`). ⚠ §15 trap: any comment near solver lines
   must not contain a bare "Cholesky" (`make_PAR.data` rewriting).

## 4. Honest scope statement

- "Machine-precision ∇·b = 0" means: the module's Green–Gauss divergence of
  the state `b` is driven to solver tolerance, everywhere including wall
  cells. It does **not** make every discrete divergence notion (e.g. the
  MPFA diffusive fluxes) vanish — that requires a face-centred /
  vector-potential (CT) reformulation of the unknown, which is a
  discretization rewrite, out of scope. The M3 doc row would be satisfied by
  this projection variant, and the docs should say so plainly.
- Residual divergence can persist at Dirichlet elements if the CL data are
  locally incompatible — reported by the bilan print, not fatal (only the
  global net-flux violation is fatal).

## 5. Validation & testing (staged per the ASN 28 gate)

1. **Unit tests** (`tests/UnitTests/`, `--arith-only-fast`): hand-built 3×3
   Cartesian patch — `Lφ == D(Gφ)` to machine precision; synthetic
   non-solenoidal field annihilated in one pass; idempotence (clean twice =
   clean once); net-flux rejection.
2. **New form `GaussLaw` + 3D twin:**
   - (a) deliberately non-solenoidal initial condition with the correction
     on → `divergence_magnetique` interior probes < 1e-12 after step 1 and
     held;
   - (b) invariance: an existing solenoidal case run with the correction on
     reproduces the baseline probes (the identity property, a strong
     no-regression statement);
   - (c) coarse/standard/fine scan of (a) to show no order degradation;
   - (d) parallel, `nbProcs=2` — the first cross-rank scalar solve in the
     module (mind the §18.2 heisenbug class);
   - (e) `ErrorInputs` additions for the incompatible-flux exit.
3. **All 28 existing forms re-run untouched** (zero code path without the
   keyword), sanitizer pass on the matrix assembly, then docs: `model.md`
   M3 row + §6 limitations, `KERNEL_REUSE_NOTES.md` §11bis Path 2 →
   implemented with the exact kernel pieces reused, `AGENTS.md`,
   `test_cases.md`, `parallel_ok[]`.

---

## 6. Implementation plan

**Mechanism:** opt-in per-step Hodge projection of `b` (the
`Navier_Stokes_std::projeter()` pattern, adapted to a cell-centred unknown),
with the self-consistent Galerkin triplet `D` / `G` / `L = D∘G` so that the
module's reported divergence (`divergence_magnetique`, and the `∇b` consumed
by `tension_magnetique` / `force_lorentz`) is driven to solver tolerance.

### 6.1 Locked decisions

| Choice               | Value                                                          |
|----------------------|----------------------------------------------------------------|
| Keyword              | `correction_divergence { }` on the magnetic equation block     |
| Default solver       | Gmres (overridable; GCP fine on Cartesian cases)               |
| Net-flux Gauss check | Fatal only when the keyword is active; no change otherwise     |

### 6.2 New / modified files

1. **`src/Correction_Divergence.{h,cpp}` (new)** — owns the operator
   triplet and the projection step:
   - `D` = Pass-1 Green–Gauss divergence (centred averages, element value at
     wall faces) — byte-identical definition to `gradient_b_elements`;
   - `G` = the matching scalar gradient; `L = D∘G` assembled once into a
     `Matrice_Morse` via `Stencil` + `Matrix_tools::allocate_morse_matrix`;
   - `projeter()`-style flow:

     ```
     secmem = D b*
       → Dirichlet-face Neumann data ∂φ/∂n = (b* − b_cl)·n
       → SolveurSys (default Gmres)
       → pin constant φ -= mp_min_vect(φ)
       → b ← b − Gφ
       → solveur_masse->corriger_solution(b, b)   # re-imposes Dirichlet
       → bilan print before/after
     ```

   - net-flux check: `∮ b_cl·n dS ≠ 0` → `Process::exit` with a "Gauss law
     violated by boundary conditions" message.
2. **`src/Convection_Diffusion_Magnetic_field.{h,cpp}`** — read
   `correction_divergence { solveur_divergence / frequence_correction / seuil_divergence }`
   via `set_param` + `lire_motcle_non_standard`; hooks:
   `mettre_a_jour(temps)` (per-step, cleans `valeurs()`),
   `preparer_calcul()` (initial cleaning of `Champ_Init`); idempotence is a
   design invariant.
3. **`src/MHD_Coupling.cpp`** — in the `couplage_fort` loop, clean `futur()`
   explicitly after each sweep (convergence measured on solenoidal
   iterates; `mettre_a_jour` alone targets the wrong slot there).
4. **Unit tests** (`tests/UnitTests/`, arith-only friendly): `Lφ == D(Gφ)`
   to machine precision on a 3×3 patch; synthetic divergence annihilated in
   one pass; idempotence; net-flux rejection.
5. **Validation forms `GaussLaw` (2D) + `GaussLaw3D` twin + `ErrorInputs`
   additions** — incl. the invariance case (solenoidal baseline + correction
   on ⇒ probes unchanged), a non-solenoidal case driven to < 1e-12, the
   mesh-convergence ladder, and `nbProcs=2` (first cross-rank scalar solve
   in the module — watch the §18.2 heisenbug class). Add to
   `parallel_ok[]`.
6. **Docs:** `model.md` M3 row → implemented (projection variant; honest
   note that MPFA-flux divergence is not the annihilated measure), §6
   limitations rewrite; `KERNEL_REUSE_NOTES.md` §11bis Path 2 → implemented
   with the reused kernel pieces; `AGENTS.md` divergence paragraph;
   `test_cases.md`.

### 6.3 Sequence

```
syntax-check loop → unit tests (--arith-only) → make module_semi_optim
  → new forms headless → full 28-form regression (must be byte-identical:
  no keyword ⇒ dead code path) → sanitizer pass on the assembly → docs
```

### 6.4 Traps to respect

- §15: no bare "Cholesky" in comment lines near solver data.
- `-Wconversion` / `-Wshadow` (face index → `int` loops, `double`
  accumulators).
- Green-baseline gate: stage per `trust-lime-testing`; the keyword must be
  opt-in everywhere, including the coupled forms.

**Next step:** rerun `GaussLaw` after the parallel ghost-potential fix, then add
the 3D twin and `ErrorInputs` coverage, then finish the docs.

---

## 7. Implementation notes (gathered 2026-10-24)

Concrete kernel APIs to reuse, verified against the sibling `trust-code`:

### 7.1 Solver reading (data surface)
- `SolveurSys` is a `OWN_PTR(SolveurSys_base)` (DERIV, not a plain class).
  Read with `is >> solveur_divergence_;` inside `lire_motcle_non_standard`,
  exactly like `Navier_Stokes_std::solveur_pression` (`is >> solveur_pression_;
  solveur_pression_.nommer("solveur_pression");`). Register via
  `param.ajouter_non_std("solveur_divergence", (this), Param::REQUIRED)`.
- Dataset keywords: `Gmres` (class `Solv_Gmres`), `GCP`, `Solv_Cholesky`,
  `Solv_Petsc` (parallel). Default per plan: `Gmres`.
- `seuil_divergence` / `frequence_correction` are plain `double`/`int` members
  read with `param.ajouter("frequence_correction", &frequence_correction_)` and
  `param.ajouter("seuil_divergence", &seuil_divergence_)`. The block keyword
  `correction_divergence { ... }` is a non-standard block parsed in
  `lire_motcle_non_standard` (return 1), reading `solveur_divergence`,
  `frequence_correction`, `seuil_divergence` in order, then the closing brace.

### 7.2 Matrix assembly (`Correction_Divergence`)
- Stencil pattern (from `Assembleur_P_PolyMAC_MPFA::assembler_mat`):
  `Stencil stencil(0, 2); stencil.append_line(e, eb); tableau_trier_retirer_doublons(stencil); Matrix_tools::allocate_morse_matrix(ne, ne_tot, stencil, mat, false);`
- Rows: local elements only (`e < ne = domaine.nb_elem()`); columns may reach
  `ne_tot = domaine.nb_elem_tot()` (ghosts), so the matrix is `ne × ne_tot`.
- Singularity handling (pure-Neumann φ): `mat(0, 0) *= 2` on the first proc
  with real elements (see `is_first_proc_with_real_elems` pattern), plus
  `φ -= mp_min_vect(φ)` after the solve (the `modifier_solution` pattern).
  Use `Process::me() == Process::mp_min(domaine.nb_elem() ? Process::me() : 1e8)`.
- `Matrice_Morse::operator()(e, eb) += coeff` for filling; `mat.sort_stencil()`
  is optional — the PolyMAC assembler calls it; mirror it for solver stability.
- Solve with `solveur_divergence_.resoudre_systeme(mat, secmem, phi)`.

### 7.3 Mass-solver re-imposition of Dirichlet values
- `Solveur_Masse_base::corriger_solution(tab_x, tab_y)` copies `y` into `x`
  only where the inverse-mass diagonal is `< 1e-12` (i.e. Dirichlet rows, where
  the mass matrix coefficient was zeroed). So it re-imposes the values passed
  in `y` at Dirichlet rows. Call it as
  `solveur_masse->corriger_solution(b, b_cl_values)` — the second argument must
  be the *target* field (the CL values), not the current field. For our case
  the current `b*` already carries the Dirichlet values at those rows from the
  convection/diffusion solve, so passing the (possibly untouched) current `b`
  is acceptable; the plan's `solveur_masse->corriger_solution(b, b)` form is
  the NS pattern and is kept for consistency.

### 7.4 Equation hooks
- `Convection_Diffusion_Temperature_base` overrides available: `mettre_a_jour(temps)`
  (Equation_base virtual, `void mettre_a_jour(double temps)`), `preparer_calcul()`
  (int, returns 1). Add `mettre_a_jour(temps)` override in the .h/.cpp and
  `preparer_calcul()` override.
- `valeurs()`/`futur()` slots: `inconnue().valeurs()` / `inconnue().futur()`
  (DoubleTab). `inconnue().temps()` gives the current time.
- `Domaine_VF` access: `ref_cast(Domaine_VF, domaine_dis())` gives `nb_elem`,
  `nb_elem_tot`, `nb_faces`, `face_voisins`, `elem_faces`, `face_normales`,
  `volumes`, `nb_faces_tot` — exactly as used by `gradient_b_elements`.

### 7.5 Picard loop (`MHD_Coupling.cpp`)
- The new solution lives in `eqb->inconnue().futur()`. Clean it after each
  sweep with `eqb->correcter_divergence_futur()` (or the equivalent in-place
  call on `futur()`), since `mettre_a_jour` targets `valeurs()`.

### 7.6 Net-flux / compatibility
- The reported divergence uses the B2-corrected gradient (boundary cells borrow
  an interior neighbour), so the reported wall-cell divergence ≈ the interior
  value. Hence the projection only needs to annihilate the Pass-1 divergence at
  *interior* cells; wall rows are extra equations (kept for a square system).
- The compatibility condition (Σ V·rhs = 0) is satisfied by the same telescoping
  as the pressure solver *provided* the boundary-face fluxes enter the RHS via
  the Dirichlet Neumann data (the "secmem trick"); pure-Neumann walls contribute
  their element-value fluxes, which telescope identically with L's wall rows.
  Validated numerically in the unit tests (stage 1).

### 7.7 Unit tests
- Framework-free kernels live in `tests/UnitTests/` with arith-only builds.
  Existing pattern: `MHD_posttraitement_kernel.h` + `unit_mhd_posttraitement_kernel.cpp`.
  The new D/G/L kernel should follow the same pattern (plain IntTab/DoubleTab
  fixtures, no mesh, no solver) so it runs under `--arith-only`.

### 7.8 Validation
- Form folder naming: `MagneticStandalone/GaussLaw/` + 3D twin
  `MagneticStandalone3D/GaussLaw3D/`, each with `src/gauss_law.data` template
  and a single `.ipynb`. Add both paths to `parallel_ok[]` in
  `check-validation.sh` (parallel exception requires the three comment-marker
  regions in the dataset).
