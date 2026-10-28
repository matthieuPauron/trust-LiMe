# Plan — Gauss' law enforcement: non-homogeneous Dirichlet magnetic data

**Status (2026-10-27):** implemented. Kernel + wrapper + gradient_b_elements wired; unit test added (`DirichletBoundaryFaceUsesPrescribedTrace`); form `GaussLawDirichlet` runs (divergence -1.816 -> 3e-14); docs updated.

## 1. Problem

The opt-in `correction_divergence` projection enforces `∇·b = 0` by solving
`(D D^T) φ = D b` and updating `b ← b − D^T φ`, where `D` is the module's
Pass-1 Green–Gauss divergence. It is verified in 2D/3D for **homogeneous
Neumann** (insulating) walls and rejects incompatible net boundary flux.

It does **not** support **non-homogeneous Dirichlet** magnetic data, i.e.
boundaries where a non-zero field `b = b₀` is prescribed
(`magnetic_field_impose`, which reuses the standard thermal `Dirichlet` CLs
because `domaine_application() == "Thermique"`). Two defects:

1. **Trace:** `D`'s boundary-face flux uses the adjacent cell value `b(e)` (zero
   normal gradient), which is only correct for Neumann walls. For a Dirichlet
   face the flux must be the prescribed `b₀·n`.
2. **Overwrite:** the projection never re-applies `imposer_cond_lim()` (doing so
   reintroduces divergence), so the correction clobbers the prescribed boundary
   values — the Dirichlet condition is silently lost.

Consequence: any form with an imposed (non-zero) magnetic field on a boundary
cannot use `correction_divergence` without corrupting its BCs.

## 2. Physical settings needing this

- Imposed external field on a wall/electrode (`b = b₀`): magnetized surfaces,
  electrodes, applied-field MHD (stirring, induction heating, liquid-metal
  batteries).
- Inlets/outlets carrying a prescribed magnetic field into the domain.
- Contrast with already-supported cases: insulating walls (homogeneous Neumann,
  `∂b/∂n = 0`) and perfect conductors (`b × n = 0`, handled as mixed
  tangential-Dirichlet-0 + normal-Neumann in `gradient_b_elements`).

## 3. Design goal

A **constrained projection** that:
- keeps every prescribed Dirichlet boundary face value `b₀` exactly (bitwise),
- still drives the module's Pass-1 Green–Gauss divergence of interior cells to
  ~0,
- remains compatible with the existing net-flux compatibility check,
- does not regress the verified Neumann/insulating cases.

## 4. Approach: split `D` into free and fixed parts

Let `D(b)` be the divergence (per cell). Decompose each boundary face's
contribution:

- **Neumann (homogeneous):** trace `= b(e)` (cell value) → depends on the cell
  → contributes to the Jacobian `J = ∂D/∂b`.
- **Dirichlet:** trace `= b₀` (prescribed) → constant → contributes to `D(b)`
  as a fixed term, **zero** to `J`.
- **Conducting (mixed):** normal component Neumann (`b(e)_n`), tangential
  components Dirichlet-0 → per-component split (see §4.4).

So `D(b) = J b + d₀`, where `d₀` collects all Dirichlet boundary-face fluxes.
The projection solves for the cell correction `δb = −J^T φ` such that
`D(b + δb) = 0`:

```
J (b + δb) + d₀ = 0  →  J δb = −(J b + d₀) = −D(b)
```
with `δb = −J^T φ` (least-norm correction in the `J^T` range):

```
J J^T φ = D(b)     (only free/Neumann rows of J contribute; Dirichlet faces
                     contribute 0 to J, hence to L = J J^T)
```

and `b ← b − J^T φ`. Dirichlet faces never enter `J`, so their `b₀` is untouched
and they enter only through `D(b)` (the RHS), which uses `b₀`.

This is exactly the existing algorithm with two changes:
- `J` (and thus `L`) is assembled **without** Dirichlet boundary-face
  contributions (they are zero in `assemble_divergence_rows`).
- `D(b)` (RHS + net-flux check + watchdog) uses the prescribed `b₀` for
  Dirichlet boundary faces instead of `b(e)`.

## 5. Kernel API changes (`src/Correction_Divergence_kernel.h`)

Introduce a per-boundary-face trace descriptor, passed as plain tables so the
kernel stays framework-free and unit-testable:

```cpp
// Boundary trace mode for a face (only meaningful when e_other < 0).
enum BCTrace { BC_NEUMANN = 0,   // trace = b(e) (current behaviour)
               BC_DIRICHLET = 1 }; // trace = prescribed value (all components)
```

New/changed signatures (add parameters; keep old overloads as thin wrappers
defaulting to all-Neumann for backward compat):

- `net_boundary_flux(..., const std::vector<int>& bc_type,   // size nf, 0/1
                     const std::vector<double>& bc_value)    // size nf*dim (only used if DIRICHLET)`
- `divergence_pass1(..., bc_type, bc_value)` — boundary face trace selection.
- `assemble_divergence_rows(..., bc_type, bc_value)` — zero contribution for
  Dirichlet boundary faces (no Jacobian dependence).
- `assemble_L` / `divergence_transpose` take the same tables (transpose uses
  the same rows, so zero contributions propagate automatically).

For a boundary face `f` with owner cell `e`:
```
if bc_type[f] == BC_DIRICHLET:
    bf_c = bc_value[f*dim + c]            // prescribed, constant
    Jacobian contribution = 0              // face ignored in J
else (Neumann):
    bf_c = b[e*nb_b + c]                  // current behaviour
    Jacobian contribution = current
```
(Conducting walls remain a separate mixed case handled in `gradient_b_elements`
for the *reported* divergence; for the correction operator they keep the
current Neumann-all-components treatment to avoid expanding scope — see §8.)

## 6. TRUST wrapper changes (`src/Convection_Diffusion_Magnetic_field.cpp`)

6.1 **Detect Dirichlet boundary faces.** In `assembler_correction()` (and a
    helper reused by `corriger_divergence`), walk `domaine_Cl_dis()`; for each
    CL, if `sub_type(Dirichlet, cl)` (covers `scalaire_impose`,
    `Champ_front_uniforme`, `Champ_front_fonc`, …), mark its faces as
    `BC_DIRICHLET` and store the prescribed values from
    `ref_cast(Dirichlet, cl).tab_val_imp(temps)` (component-wise, evaluated at
    the current time). Conducting walls keep `BC_NEUMANN` in the operator (mixed
    handling deferred; see §8).

    Store in new members: `std::vector<int> bc_trace_type_` (size
    `nb_faces_tot`), `std::vector<double> bc_trace_value_` (size
    `nb_faces_tot * dim`), and a `bool has_dirichlet_bc_` flag. Rebuild these
    when `mettre_a_jour` runs if time-dependent CLs are used (values may
    change with time).

6.2 **`assembler_correction()`** passes `bc_trace_type_` / `bc_trace_value_` to
    `Correction_Divergence_kernel::assemble_L`. Dirichlet faces contribute
    zero to `L`, so `L` may become (more) rank-deficient on meshes where a whole
    component is Dirichlet-dominated; the existing `1e-12` relative diagonal
    regularization already handles redundant rows. Verify the regularization
    still suffices (§9).

6.3 **`corriger_divergence()`**:
    - Pass `bc_trace_type_` / `bc_trace_value_` to `net_boundary_flux` (so the
      compatibility check uses prescribed `b₀`).
    - Pass them to `divergence_pass1` (RHS uses prescribed values).
    - `divergence_transpose` uses the same rows → Dirichlet faces contribute
      zero to the correction `g_flat`, so prescribed boundary cells are not
      touched by that face's contribution. (Interior coupling still updates
      boundary-adjacent cells through shared interior faces — intended.)
    - The watchdog `divergence_pass1` uses the same tables.

6.4 **`mettre_a_jour()` / `preparer_calcul()`** refresh `bc_trace_value_` when
    CLs are time-dependent before calling `corriger_divergence`.

6.5 **`gradient_b_elements()`** already uses `b(e)` for Neumann and `0` for
    conducting tangential. For the *reported* divergence to match the projection's
    `D`, the reported operator should also use prescribed `b₀` on Dirichlet
    boundary faces. Update the boundary-face trace in Pass 1 to consult
    `bc_trace_type_`/`bc_trace_value_` (Dirichlet → prescribed value). This
    keeps `divergence_magnetique` consistent with the projection. Note:
    `gradient_b_elements` runs before correction is necessarily assembled, so
    the BC tables must be built early (in `preparer_calcul` or on first use).

## 7. Validation / compatibility

- Net flux compatibility: `net_boundary_flux` sums `b_face·n` over physical
  boundary faces. With Dirichlet, `b_face = b₀`, so the check becomes "the
  prescribed boundary flux must be compatible (≈0 net)" — same condition,
  correct for a solenoidal field with those BCs.
- Watchdog `seuil_divergence_` unchanged.

## 8. Explicitly out of scope (deferred)

- **Mixed conducting walls in the correction operator:** `paroi_conductrice_magnetique`
  (tangential `b = 0`, normal Neumann) is currently treated as all-Neumann in
  `corriger_divergence` while `gradient_b_elements` treats it as conducting.
  This pre-existing inconsistency is not fixed here; a follow-up can extend the
  trace descriptor to per-component modes. The Dirichlet work adds full-face
  Dirichlet only.
- **Vector-potential / Nedelec formulation:** separate deferred approach.
- **Restart / calibration / physical validation** of the magnetic k-ε closure:
  unrelated.

## 9. Risks

- **Rank deficiency / regularization:** zeroing Dirichlet rows of `J` removes
  contributions; on meshes where a component is entirely Dirichlet-bounded, `L`
  may be more singular. The `1e-12 * max_diagonal` shift should still
  regularize, but must be re-checked (unit + form tests).
- **Time-dependent CLs:** `tab_val_imp(temps)` must be refreshed; forgetting
  this gives stale `b₀`. Handled in §6.4.
- **MPI / ghosts:** BC tables sized `nb_faces_tot` (includes ghosts) so
  parallel indexing stays safe, matching existing `is_cond_face` sizing.
- **Reported vs. projected divergence consistency:** §6.5 keeps them aligned;
  a mismatch would make the watchdog meaningless.

## 10. Implementation order

1. Kernel: extend `Correction_Divergence_kernel.h` with `bc_type`/`bc_value`
   params + `BC_NEUMANN`/`BC_DIRICHLET`; update `divergence_pass1`,
   `net_boundary_flux`, `assemble_divergence_rows` (and thus `assemble_L`,
   `divergence_transpose`). Keep all-Neumann overloads for backward compat.
2. Add arith-only unit tests for a Dirichlet boundary face (prescribed trace,
   zero Jacobian contribution, net-flux check uses prescribed value).
3. Wrapper: add `bc_trace_type_`/`bc_trace_value_` members + builder; wire into
   `assembler_correction`, `corriger_divergence`, watchdog.
4. Refresh BC tables in `mettre_a_jour`/`preparer_calcul`.
5. Update `gradient_b_elements` Pass 1 boundary trace for Dirichlet consistency.
6. Form-level validation: add a small case with an imposed `b₀` on one wall +
   `correction_divergence`, check (a) boundary values preserved bit-for-bit,
   (b) interior divergence below `seuil`, (c) net flux compatible.
7. Update docs: `model.md` §1/§6, `kernel_reuse.md` §11bis, `plans/gauss_law.md`
   §7.3, `NEXT.md` (move item to completed with evidence).

## 11. Test plan

- Arith-only: unit tests in `tests/UnitTests/unit_correction_divergence_kernel.cpp`
  for a 1D two-cell domain with a Dirichlet face: verify `divergence_pass1`
  uses prescribed value, `assemble_L` has zero in the corresponding column, and
  the projected field preserves the prescribed boundary trace.
- Form: new/extended validation notebook case (insulating-wall twin with one
  wall switched to `magnetic_field_impose`) checking preservation + divergence.
- Regression: re-run the existing 2D/3D Gauss-law scans (homogeneous Neumann)
  to confirm no change (all-Neumann path identical).

## 12. Open questions

- Should Dirichlet values be read at `preparer_calcul` time only, or refreshed
  every `mettre_a_jour`? If a `Champ_front_fonc` depends on time/space, refresh
  each step (§6.4). Assume yes.
- Is the `1e-12` regularization adequate when a whole face set is Dirichlet?
  To be measured in §9; may need a stronger (e.g. pin) strategy.
- Should the correction also handle the conducting-wall mixed case now, or keep
  deferred? Recommendation: keep deferred to limit risk; revisit after Dirichlet
  lands.
