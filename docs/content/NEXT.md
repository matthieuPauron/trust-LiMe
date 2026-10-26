@page TrustLiMe_next Next Steps and Task Backlog

This page is the **living backlog** for TRUST-LiMe: which tasks are done, which
are in progress, and which are prescribed for the coming weeks. Update it from
verified repository state; do not carry forward tasks already completed or
superseded by newer validation evidence.

@note This page follows the planning documents under `docs/content/plans/` and
`docs/content/end_goal.md`; those remain the working documents. This page is
the consolidated, user-facing view.

---

## 1. Completed (recent)

| Task | Evidence |
|---|---|
| Standalone magnetic induction equation (M0/M1) | `Pb_Induction`, `MagneticStandalone/` forms |
| Coupled low-Rm mode (one- and two-way Lorentz force) | `Pb_MHD`, `force_lorentz`, `vitesse_hydraulique` |
| Post-treatment fields (`Champ_Fonc_MHD`) | merged into `Champs2D` / `Champs3D` |
| Gauss-law projection (opt-in, 2D/3D) | `docs/content/plans/gauss_law.md`, `correction_divergence` |
| Turbulent magnetic diffusivity paths | prescribed fields, hydraulic k-ε, independent magnetic k-ε |
| Strong two-way coupling (Picard, opt-in) | `couplage_fort` |
| `HartmannTwoWay` targeted rerun | all 18 cases passed (2026-10-26) |
| Solved spatial-transport verification for magnetic k-ε | `MagneticTurbulence/` solved spatial-transport case (nonuniform k_b/ε_b, positivity/finite/convergence) |
| Extend magnetic k-ε coverage (partial) | energy-transfer case + molecular twin in `MagneticTurbulence/` (23 cases pass); restart, ladder, VEF remain open |

## 2. In progress

| Task | Status | Notes |
|---|---|---|
| Magnetic k-ε verification and validation | partial | frozen manufactured terms, zero-production analytic decay, solved spatial transport, and a solved energy-transfer case with a molecular twin pass on PolyMAC_P0; VEF coverage, restart, calibration and physical validation remain open |
| Gauss-law sanitizer follow-up | open | 2D/3D forms pass; 38 arithmetic-only and 74 full unit tests pass, but sanitizer coverage is not recorded |
| Gauss-law non-homogeneous Dirichlet support | open | unconstrained projection does not preserve non-homogeneous Dirichlet magnetic data; needs a constrained operator |

@note The full reportable suite report is generated nightly. The last full
pre-merge report was 31/32 (30/31 after merging the two Shercliff forms into
one); the targeted `HartmannTwoWay` rerun has passed since then. No manual full
report run is required.

## 3. Prescribed for the next weeks

Priorities below are ordered by closure value and dependency.

1. **Run sanitizer coverage** — ASan/UBSan remains open, especially for the divergence-projection assembly and full-kernel interactions.
2. **Add remaining 3D twins and an inertial-Re benchmark** — `ShercliffDuct3D` is already present and passing; full-suite status is tracked by the nightly report.
3. **Long term: advanced coupled physics and VKS** — magnetic fluctuation transport/full turbulent MHD closure, Ohmic heating, high-Rm nonlinear coupling, dynamo threshold and saturation; see `docs/content/end_goal.md`.

## 4. Proposed by agent (awaiting validation)

Tasks proposed by the nightly backlog agent after completing a task. They are
**not** executed by the cron job until a human validates them (moves them into
section 3, or discards them). Each entry should state the rationale and the
evidence that motivated it.

_(none yet)_

## 5. How this page is maintained

- Reconcile this page with `docs/content/plans.md`, validation records and
  `AGENTS.md` whenever project status changes.
- A passing formatting check is not evidence that a validation computation
  passed; distinguish formatting, targeted runs and the full reportable sweep.
