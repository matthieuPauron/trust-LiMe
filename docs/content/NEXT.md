@page TrustLiMe_next Next Steps and Task Backlog

This page is the **living backlog** for TRUST-LiMe: which tasks are done, which
are in progress, and which are prescribed for the coming weeks. It is regenerated
nightly by the `trust-lime-docs-coherence` Hermes cron job, which checks the
tasks below against the repository state and appends new tasks derived from the
gaps highlighted elsewhere in the documentation.

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

## 2. In progress

| Task | Status | Notes |
|---|---|---|
| `HartmannTwoWay` validation case | failed at `run.reset()` on non-empty scan dir | resume after cleanup |
| Full 45-test GoogleTest run | SIGSEGV at first kernel-linked registration test | `--arith-only` (29 tests) passes |
| Magnetic k-ε closure smoke case | aborts at `Solve` | source-level only |
| Gauss-law full-suite / sanitizer follow-up | documented | see `docs/content/plans/gauss_law.md` |

## 3. Prescribed for the next weeks

Tasks derived from gaps highlighted in the documentation and open items in
`AGENTS.md`. The nightly job re-evaluates and refreshes this list.

1. **Close validation gaps** — fix the two reported validation-check violations
   (missing `# BEGIN MESH #` marker in `Fusion_Blanket`; sequential-only
   compliance in `VKSTurbulent`).
2. **Full baseline regression** — run the complete 32-form suite after the recent
   VEF/VKS coupling changes; refresh PDF sweep.
3. **Unit-test sanitizer** — resolve the SIGSEGV in the full GoogleTest run so
   all 45 tests execute.
4. **3D coupled duct case** — add the missing 3D coupled duct validation form.
5. **Turbulence comparison form** — add molecular-vs-k-ε comparison to
   `TurbulentDiffusion`.
6. **Documentation coherence** — keep this page and `plans.md` aligned with
   `PLAN_*.md` and `AGENTS.md` (automated nightly).
7. **High-Rm dynamo / VKS** — long-term; see `docs/content/end_goal.md`.

## 4. How this page is maintained

- Updated nightly by the `trust-lime-docs-coherence` Hermes cron job.
- The job checks each task above against the repository (commits, sources,
  validation forms) and marks completed items, then prescribes new tasks from
  documented gaps.
- Manual edits are welcome; the nightly run reconciles them with project state.