@page TrustLiMe_plan_gausslaw Plan: Gauss' law enforcement

# Plan — Gauss' law enforcement (Path 2: opt-in divergence correction)

**Status (2026-10-24): 2D/3D validation and incompatible-flux rejection pass.**
The implementation uses the exact sparse transpose of the Pass-1 divergence
(`L = D D^T`), with a `1e-12` relative diagonal regularization, and sums the
boundary-flux compatibility check across MPI while counting only owned
physical faces. The 2D two-rank notebook passes coarse/standard/fine with
post-initialization probe maxima `1.74e-13`, `5.79e-14`, and `4.75e-14`. The
sequential 3D twin passes at `1.17e-14`, `4.64e-14`, and `1.43e-13`; it uses an
x-only full-vector field and three cells in y/z to avoid a parity-sensitive
3D null mode while retaining the shared active-x refinement ladder. `ErrorInputs` also
confirms rejection of incompatible net boundary flux. The full PDF sweep ran
31/32 reportable forms; its only failure was the in-progress `HartmannTwoWay`
scan calling `run.reset()` on a non-empty generated directory. The full
45-test GoogleTest run currently SIGSEGVs before its first test; all 29
arith-only tests pass. Sanitizer validation remains open.

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
- Without the opt-in `correction_divergence` block, that divergence remains an
  assumption (Path 1, `docs/content/dev_corner/kernel_reuse.md` §11bis). With the block enabled,
  the implementation projects the field in this discrete measure; it does not
  enforce every flux-based divergence associated with other discretizations.

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

## 3. Implemented design — `correction_divergence { }` (opt-in Path 2)

A per-step magnetic Hodge projection owned by
`Convection_Diffusion_Magnetic_field`, mirroring `projeter()` step for step:

1. **Exact discrete projection.** `D` is the Pass-1 Green–Gauss divergence
   (centred face averages inside, adjacent element value at physical walls),
   implemented in `src/Correction_Divergence_kernel.h`. The correction is the
   exact algebraic transpose `D^T`, not another cell-centred Green–Gauss
   reconstruction. Assemble `L = D D^T` once as a `Matrice_Morse` and solve
   `Lφ = D b*`; then `b ← b − D^Tφ` gives `D b_new = D b* − D D^Tφ` up to
   solver tolerance and the documented diagonal regularization. The initial
   `D∘G` design developed checkerboard null modes and did not consistently
   reproduce the reported divergence operator.

2. **Boundary conditions.** TRUST applies equation boundary conditions before
   the per-step projection. Re-imposing them after projection changed the
   field and reintroduced divergence, so the projection does not call
   `imposer_cond_lim()` afterward. The current validation uses homogeneous
   Neumann walls. Preserving non-homogeneous Dirichlet magnetic data remains
   unsupported and needs a constrained operator.

3. **Net-flux contract.** The implementation checks global boundary-flux
   compatibility before projection. It counts owned physical faces only and
   MPI-sums net flux and its absolute scale; joint/ghost faces are excluded.
   Nonzero net flux exits with a clear Gauss-law message.

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

- "Small ∇·b" means: the module's Pass-1 Green–Gauss divergence of the state
  `b` is driven to solver tolerance, plus the `1e-12` relative operator
  regularization, including physical boundary cells. It does **not** make every discrete divergence notion (e.g. the
  MPFA diffusive fluxes) vanish — that requires a face-centred /
  vector-potential (CT) reformulation of the unknown, which is a
  discretization rewrite, out of scope. The M3 doc row would be satisfied by
  this projection variant, and the docs should say so plainly.
- Non-homogeneous Dirichlet magnetic data are not preserved by this
  unconstrained projection and are not yet supported.

## 5. Validation & testing (staged per the ASN 28 gate)

1. **Unit tests — implemented:** `tests/UnitTests/unit_correction_divergence_kernel.cpp`
   covers a hand-built 3×3
   Cartesian patch — `Lφ == D(D^Tφ)` with reversed boundary orientations and
   nonuniform volumes; synthetic
   non-solenoidal field annihilated in one pass; idempotence (clean twice =
   clean once); net-flux rejection.
2. **`GaussLaw` 2D form — passes** on 2 ranks, coarse/standard/fine. It covers:
    - deliberately non-solenoidal initial condition with the correction
      on → `divergence_magnetique` interior probes < 1e-12 after step 1 and
      held;
    - coarse/standard/fine mesh scan on 2 MPI ranks.
3. **Validation coverage complete:** the 3D twin and incompatible-net-flux
   `ErrorInputs` assertion pass. The full PDF sweep ran 31/32 reportable forms;
   the only failure is the unrelated in-progress `HartmannTwoWay` scan reset.
   The full kernel-linked unit run and sanitizer validation remain open.
   Synchronize `docs/content/dev_corner/kernel_reuse.md` §11bis and `AGENTS.md`.

---

## 6. Implementation plan

**Mechanism:** opt-in per-step Hodge projection of `b` (the
`Navier_Stokes_std::projeter()` pattern, adapted to a cell-centred unknown),
with the exact discrete operator `D` / `D^T` / `L = D D^T` so that the
module's reported divergence (`divergence_magnetique`, and the `∇b` consumed
by `tension_magnetique` / `force_lorentz`) is driven to solver tolerance.

### 6.1 Locked decisions

| Choice               | Value                                                          |
|----------------------|----------------------------------------------------------------|
| Keyword              | `correction_divergence { }` on the magnetic equation block     |
| Default solver       | Gmres (overridable; GCP fine on Cartesian cases)               |
| Net-flux Gauss check | Fatal only when the keyword is active; no change otherwise     |

### 6.2 New / modified files

1. **`src/Correction_Divergence_kernel.h` (new)** — framework-free sparse
   rows for the Pass-1 divergence and its transpose:
   - `D` = Pass-1 Green–Gauss divergence (centred averages, element value at
     wall faces) — byte-identical definition to `gradient_b_elements`;
   - `D^T` = exact algebraic transpose; `L = D D^T` assembled once into a
     `Matrice_Morse` via `Stencil` + `Matrix_tools::allocate_morse_matrix`;
   - `projeter()`-style flow:

     ```
     secmem = D b*
        → SolveurSys (default Gmres)
        → b ← b − D^Tφ
       → bilan print before/after
     ```

    - global net-flux check: `∮ b·n dS ≠ 0` → `Process::exit` with a "Gauss law
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
4. **Unit tests** (`tests/UnitTests/`, arith-only friendly): `Lφ == D(D^Tφ)`
   to machine precision on a 3×3 patch; synthetic divergence annihilated in
   one pass; idempotence; net-flux rejection.
 5. **Validation — implemented:** the merged `MagneticStandalone/GaussLaw/`
    notebook covers 2D non-solenoidal cleanup and the MPI mesh ladder, plus
    the 3D full-vector divergence assertion sequentially on odd x-cell counts. The merged
   `ErrorInputs` form asserts clean rejection of incompatible net flux.
6. **Docs — updated:** `model.md`, `test_cases.md`, `docs/content/plans.md`,
   `docs/content/validation.md`, `docs/content/dev_corner/kernel_reuse.md` §11bis and `AGENTS.md`
   document the mechanism, coverage and remaining regression/sanitizer gates.

### 6.3 Sequence

```
syntax-check loop → unit tests (--arith-only) → make optim
  → new forms headless → full 32-form reportable regression (must be byte-identical:
  no keyword ⇒ dead code path) → sanitizer pass on the assembly → docs
```

### 6.4 Traps to respect

- §15: no bare "Cholesky" in comment lines near solver data.
- `-Wconversion` / `-Wshadow` (face index → `int` loops, `double`
  accumulators).
- Green-baseline gate: stage per `trust-lime-testing`; the keyword must be
  opt-in everywhere, including the coupled forms.

**Next step:** resolve the full kernel-linked unit-test SIGSEGV, rerun the
complete reportable suite after the in-progress `HartmannTwoWay` form is
cleanable, and run sanitizers on the matrix assembly.

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

### 7.2 Matrix assembly (`D D^T`)
- Stencil pattern (from `Assembleur_P_PolyMAC_MPFA::assembler_mat`):
  `Stencil stencil(0, 2); stencil.append_line(e, eb); tableau_trier_retirer_doublons(stencil); Matrix_tools::allocate_morse_matrix(ne, ne_tot, stencil, mat, false);`
- Rows: local elements only (`e < ne = domaine.nb_elem()`); columns may reach
  `ne_tot = domaine.nb_elem_tot()` (ghosts), so the matrix is `ne × ne_tot`.
- The operator can have redundant rows. Add a `1e-12` relative diagonal
  regularization; do not pin a row or shift `φ` afterward.
- `Matrice_Morse::operator()(e, eb) += coeff` for filling; `mat.sort_stencil()`
  is optional — the PolyMAC assembler calls it; mirror it for solver stability.
- Solve with `solveur_divergence_.resoudre_systeme(mat, secmem, phi)`.

### 7.3 Boundary limitation
- Reapplying `imposer_cond_lim()` after projection changed the field and
  reintroduced divergence in the Neumann-wall validation. The code therefore
  does not re-impose boundary conditions afterward; non-homogeneous Dirichlet
  magnetic data need a constrained projection before being supported.

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
- Only owned physical faces (`f < domaine.nb_faces()`) contribute to the
  boundary-flux check. MPI-sum local net flux and absolute scale before testing
  compatibility; ghost/joint faces are not physical boundary contributions.

### 7.7 Unit tests
- Framework-free kernels live in `tests/UnitTests/` with arith-only builds.
  Existing pattern: `MHD_posttraitement_kernel.h` + `unit_mhd_posttraitement_kernel.cpp`.
  The D/D^T/L kernel follows the same pattern (plain connectivity vectors
  fixtures, no mesh, no solver) so it runs under `--arith-only`.

### 7.8 Validation
- The single `MagneticStandalone/GaussLaw/` form uses one shared
  `src/gauss_law.data` template and notebook for both dimensions. The 2D case
  is allowlisted in `check-validation.sh` and uses the parallel marker regions;
  the 3D case runs sequentially. Its x scan uses the shared 16/32/64-cell
  ladder and three cells in y/z, avoiding the 3D parity-sensitive null mode.
