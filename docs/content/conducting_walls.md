@page TrustLiMe_conducting_walls Conducting-wall boundary conditions

This page records the design and implementation plan for **conducting-wall
boundary conditions** on the magnetic induction equation. It is the working
document for the deferred item tracked as Phase B.2 in
@ref TrustLiMe_plans "Roadmap and next steps".

---

> **Status (2026-10-25): implemented and runtime-validated.** Phase 9 validation
> cases (`MagneticStandalone/ConductingWall`, sequential) run to completion for
> both the scalar and vector models; existing insulating-wall cases
> (`Hartmann1D`) re-run without regression.

## 1. What "conducting wall" means here

The module already ships an **insulating** wall, `paroi_isolante_magnetique`
(homogeneous Neumann on every component of \f$\mathbf b\f$). The conducting
counterpart is the **perfectly conducting** wall:

\f[
  \mathbf b \times \mathbf n = 0
\f]

i.e. the **tangential** components of \f$\mathbf b\f$ are clamped to Dirichlet 0,
while the **normal** component keeps a zero flux (Neumann). This is a mixed,
per-component, normal-dependent condition -- not a pure Neumann condition and not
a pure Dirichlet one.

A **finite-conductivity (thin) wall** is the more general case: the tangential
components satisfy a Robin condition governed by the wall conductance, with the
normal component still Neumann. It is deliberately deferred; see §6.

## 2. Why this is not a one-line alias

`paroi_isolante_magnetique` is a thin subclass of `Neumann_paroi_adiabatique`
because one condition (Neumann) applies uniformly to every component. A
conducting wall needs Dirichlet on the tangential components and Neumann on the
normal one, and the tangential/normal split depends on the face normal. TRUST's
operators classify a whole boundary-condition object as a single type, so a
subclass alone cannot express the mix -- the PolyMAC (and VEF) operators have to
learn the new type and branch per component.

## 3. Implementation plan (perfect conductor)

### Phase 1 -- Module CL class (`src/`)

Add `src/Paroi_conductrice_magnetique.{h,cpp}`:

- derive from `Cond_lim_base`;
- register under the keyword `paroi_conductrice_magnetique`;
- in `readOn`, set `app_domains = {Magnetique, Thermique, indetermine}` so
  `Cond_lim_base::compatible_avec_eqn()` accepts it on the magnetic equation;
- store no value (homogeneous).

Add a header-only, unit-testable helper returning, for a face normal
\f$\mathbf n\f$ and `nb_composantes == dimension`, the index of the component
aligned with the normal (argmax \f$|n_d|\f$). For `nb_composantes < dimension`
(scalar model) treat every component as tangential.

### Phase 2 -- PolyMAC boundary face values

In `Domaine_Cl_PolyMAC_family::imposer_cond_lim`, for a
`Paroi_conductrice_magnetique` boundary, set the boundary face value of every
**tangential** component to 0 and leave the **normal** component untouched
(Neumann/extrapolation), mirroring the existing `Dirichlet_paroi_fixe` branch.

### Phase 3 -- PolyMAC CL classification

In `Iterateur_PolyMAC_CDO_base::type_cl()` (and the MPFA/HFV equivalents), add a
branch mapping `Paroi_conductrice_magnetique` to a new enum value
`paroi_conductrice`, and add that value to the `Type_Cl_PolyMAC_*` enums.

### Phase 4 -- PolyMAC diffusion operators

In `Op_Diff_PolyMAC_*_Face.cpp` and `*_Elem.cpp`, for `paroi_conductrice` apply
the Dirichlet-0 flux treatment to tangential components and the Neumann-0
treatment to the normal component, branching per component on
`c == normal_component_index(face_normal)`.

### Phase 5 -- PolyMAC convection operators

For `paroi_conductrice`, use 0 for tangential boundary values and the interior
value for the normal component.

### Phase 6 -- Mass matrix / pressure solve

In `Masse_PolyMAC_*_Face.cpp`, treat tangential components on
`paroi_conductrice` faces as Dirichlet faces; normal components keep the
Neumann treatment.

### Phase 7 -- Module gradient reconstruction

`Convection_Diffusion_Magnetic_field::gradient_b_elements()` (Pass 1) currently
assumes a zero normal gradient for **all** components at every boundary. For
conducting walls this is wrong for the tangential components, whose boundary
value is 0. Update Pass 1 to use 0 for tangential components on
`Paroi_conductrice_magnetique` faces and the cell value for the normal
component. This affects `force_lorentz`, `courant_magnetique`,
`rotationnel_magnetique` and the Gauss-law projection.

### Phase 8 -- VEF support

Mirror Phases 2--6 in `trust-code/src/VEF` (`Domaine_Cl_VEF`,
`Op_Dift_*_VEF_*`, `Op_Conv_*`, `Masse_VEF_*`). VEF is required for the
turbulent validation cases. VDF is not used by any trust-LiMe case and is
skipped.

### Phase 9 -- Documentation and validation

- Move `paroi_conductrice_magnetique` from "deferred (Phase B.2)" to
  "implemented" in `docs/content/model.md` §4.
- Update `docs/content/plans.md` §3 and `docs/content/dev_corner/kernel_reuse.md` item 9.
- Add a validation case: a Hartmann-type channel with conducting side walls,
  with 2D and 3D twins, run sequentially per the suite rules.

## 4. Testing strategy

1. Unit test the normal-component-index helper (argmax, ties, scalar fallback).
2. `syntax-check.sh` after every meaningful edit.
3. Build `module_semi_optim`, run the new case, compare the induced field
   profile to the analytic perfect-conductor solution.
4. Re-run the existing insulating-wall cases as a regression check.

## 5. Risks

- `-Wconversion` / `-Werror`: new kernel code must avoid implicit conversions.
- The Phase 7 change touches the Lorentz source and the Gauss-law projection;
  validate both with and without conducting walls.
- Parallel correctness: handle ghost elements consistently with the existing
  `ne_tot`/`ne` patterns.

## 6. Why finite-conductivity (Robin) walls are deferred -- and why they matter

A perfect conductor is the \f$C \to \infty\f$ limit of a wall of finite
conductance \f$C = \sigma_w t_w / (\sigma_f a)\f$. Real walls (steel, copper,
aluminium in liquid-metal loops) have finite \f$C\f$, and the transition from
insulating (\f$C = 0\f$) to perfect conductor (\f$C = \infty\f$) is continuous
and physically important:

- **Experimental validation**: once comparison is made against sodium/mercury/
  gallium experiments or published MHD duct benchmarks (Shercliff, Hunt, Tassone),
  the wall conductance is a primary parameter. A perfect-conductor-only code can
  only hit the two endpoints of those parametric curves.
- **Pressure drop and field screening** depend smoothly on \f$C\f$; the perfect-
  conductor limit over-predicts screening and can mis-estimate pressure drops in
  real components.
- **VKs / industrial goals** (`docs/content/end_goal.md`): impeller housings, ducts and
  blankets are not superconductors; their finite conductance affects the dynamo
  threshold and the saturated field.

Implementing finite conductivity later means generalising the same mixed CL with a
`conductance` parameter (scalar or `Champ_Don`): \f$\infty\f$ recovers the
perfect conductor, \f$0\f$ recovers the insulating wall, finite values give the
Robin flux \f$\partial b_t/\partial n = (1/\delta)\,b_t\f$. Because the
per-component, normal-aware operator plumbing is identical, the incremental cost
after this plan lands is small -- which is why doing the perfect-conductor case
first is the right sequencing. The recommended end state is a single unified CL
with a `conductance` parameter, keeping one code path and one validation matrix.

## 7. Open questions

1. Should the normal component be Neumann (this plan) or Dirichlet 0 (strict
   \f$\mathbf b \cdot \mathbf n = 0\f$)? The latter is the other deferred item.
2. Is a Hartmann channel with conducting side walls an acceptable first
   validation case, or is a Shercliff duct preferred?
3. Should the finite-conductivity (Robin) generalisation be a follow-up to this
   plan or planned jointly? (Recommendation: follow-up, to keep this change
   reviewable.)