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

## 2. In progress

| Task | Status | Notes |
|---|---|---|
| Magnetic k-ε verification and validation | partial | frozen manufactured terms and zero-production analytic decay pass on PolyMAC_P0; solved spatial transport, VEF coverage, restart, calibration and physical validation remain open |
| Gauss-law sanitizer follow-up | open | 2D/3D forms pass; 38 arithmetic-only and 74 full unit tests pass, but sanitizer coverage is not recorded |

@note The full reportable suite report is generated nightly. The last full
pre-merge report was 31/32 (30/31 after merging the two Shercliff forms into
one); the targeted `HartmannTwoWay` rerun has passed since then. No manual full
report run is required.

## 3. Prescribed for the next weeks

Priorities below are ordered by closure value and dependency.

1. **Add solved spatial-transport verification for magnetic k-ε** — the current manufactured-term and analytic-decay checks do not exercise evolving nonuniform k-ε fields under advection and diffusion.
2. **Extend magnetic k-ε coverage** — add energy-transfer and restart checks, a coarse/standard/fine ladder, and the planned molecular-versus-k-ε comparison; then pursue VEF and physical validation.
3. **Run sanitizer coverage** — ASan/UBSan remains open, especially for the divergence-projection assembly and full-kernel interactions.
4. **Add remaining 3D twins and an inertial-Re benchmark** — `ShercliffDuct3D` is already present and passing; full-suite status is tracked by the nightly report.
5. **Long term: advanced coupled physics and VKS** — magnetic fluctuation transport/full turbulent MHD closure, Ohmic heating, high-Rm nonlinear coupling, dynamo threshold and saturation; see `docs/content/end_goal.md`.

## 4. How this page is maintained

- Reconcile this page with `docs/content/plans.md`, validation records and
  `AGENTS.md` whenever project status changes.
- A passing formatting check is not evidence that a validation computation
  passed; distinguish formatting, targeted runs and the full reportable sweep.
