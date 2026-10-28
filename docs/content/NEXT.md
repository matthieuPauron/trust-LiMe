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
| Inertial-Re benchmark (HartmannDeveloping) | validated | `MagneticCoupled/HartmannDeveloping`: finite duct, plug inlet, outlet with Hartmann pressure profile; coarse/standard/fine mesh scan runs; outlet u converges (5.5e-4 std->fine); mass-flow conserved (err ~5e-3->3.5e-3); monotonic development; centerline is max; time-converged (drift < 1e-6). Profile is a developing entry flow (development ratio 0.69->0.64); ~28% normalized shape error vs mass-flow-fitted Hartmann (upwind numerical diffusion + entry physics). |

@note The full reportable suite report is generated nightly. The last full
pre-merge report was 31/32 (30/31 after merging the two Shercliff forms into
one); the targeted `HartmannTwoWay` rerun has passed since then. No manual full
report run is required.

## 3. Prescribed for the next weeks

Priorities below are ordered by closure value and dependency, following the
ASNR guide 28 validation ordering: software-level improvements first, then
verification/validation, then full physical cases.

1. **Run sanitizer coverage** — ASan/UBSan remains open, especially for the divergence-projection assembly and full-kernel interactions. *(software)*
2. **VEF port of magnetic k-ε closure** — PolyMAC-only today; hydraulic k-ε handoff is VEF-only, so the closures cannot run on the same mesh. VEF variants `Champ_Fonc_Eta_Turbulent_VEF` and `Champ_Fonc_Somme_VEF` now created and compile (base `Champ_Fonc_P0_VEF`); equation-side wiring to select them by discretization remains. *(software)*
3. **Gauss-law sanitizer follow-up** — 2D/3D forms pass; 38 arithmetic-only and 74 full unit tests pass, but sanitizer coverage is not recorded. *(software)*
4. **Gauss-law non-homogeneous Dirichlet support** — implemented (constrained projection + unit test + form `GaussLawDirichlet` runs: divergence -1.816 -> 3e-14). *(software)*
5. **GPU/Kokkos port — sub-task 1: build & verify** — build a GPU-enabled kernel+module (`-cuda=<ccXX>`), rebuild trust-LiMe against it, verify a small VEF case runs on GPU and matches CPU numerically. *(software; part of the larger proposed task in section 4)*
6. **Add remaining 3D twins and an inertial-Re benchmark** — 3D coupled twins (`HartmannTwoWay3D`, `ForceLorentzBalance3D`, `Coupled_Feedback3D`, `ShercliffDuct3D`) already present and recorded passing (2026-10-26). Inertial-Re benchmark `MagneticCoupled/HartmannDeveloping` validated: mass-flow conservation, monotonic development, centerline maximum, time convergence, mesh convergence all pass; developing entry flow (development ratio 0.69->0.64), ~28% shape error vs mass-flow-fitted Hartmann (upwind diffusion + entry physics). *(physical verification)*
7. **Complete magnetic k-ε verification** — restart/save-restore check, full coarse/standard/fine ladder for manufactured terms, calibration and physical validation. *(physical verification)*
8. **Long term: advanced coupled physics and VKS** — magnetic fluctuation transport/full turbulent MHD closure, Ohmic heating, high-Rm nonlinear coupling, dynamo threshold and saturation. *(full physical cases)*
9. **Industrial example: EM_Stirring** — replace placeholder hydrostatic balance with a real continuous-casting stirring geometry (coil-driven rotating field, free surface, meniscus) and validation case.
10. **Industrial example: Crystal_Growth** — magnetic damping of melt convection in a Czochralski crucible (real geometry, temperature field, damping factor vs Ha).
11. **Industrial example: Fusion_Blanket** — LiPb MHD in a breeding-blanket channel with fringing field and pressure drop (Shercliff/Hunt benchmark).
12. **Industrial example: EM_Pump** — Lorentz-driven annular/linear pump with real flow rate vs current characteristic.
13. **Industrial example: Al_Electrolysis** — Hall-Héroult cell MHD: interfacial waves, anode-cathode distance, metal pad roll.
14. **Industrial example: LM_Battery** — layered liquid-metal battery: Tayler instability / interface stability under current.
15. **Industrial example: SFR_Duct** — sodium duct MHD pressure drop with conducting walls (Hunt/Shercliff benchmark).

## 4. Proposed by agent (awaiting validation)

Tasks proposed by the nightly backlog agent after completing a task. They are
**not** executed by the cron job until a human validates them (moves them into
section 3, or discards them). Each entry should state the rationale and the
evidence that motivated it.

### Proposed: GPU/Kokkos port of trust-LiMe (LARGE — split before execution)

**Rationale:** trust-LiMe runs on a 4-thread HDD machine with few physical
cores; domain decomposition is limited. TRUST (kernel 1.9.8) is
progressively ported to GPU via Kokkos (CUDA/ROCm) and VEF is GPU-supported
since 1.9.6, with GPU-native solvers (PETSc/AMGX, cuDSS). Porting
trust-LiMe to Kokkos would let the module offload work to an Nvidia GPU
instead of relying on many MPI ranks.

**Evidence:** `docs/content/user_guide/general_guide/intro.md` (GPU/Kokkos,
VEF on GPU in 1.9.6); `configure -cuda/-kokkos_openmp`; `tests/GPU/` VEF
cases; `TRUSTArray`/`TRUSTTab` `view_rw`/`view_ro` + `Kokkos::parallel_for`;
`AGENTS.md` (C++20 required for Kokkos).

**This is a very large task and must NOT be executed as a single nightly
task.** Split into (at least) the following sub-tasks, each validated
independently before the next is started:

1. Build a GPU-enabled kernel + module (configure trust-code with
   `-cuda=<ccXX>`, build `TRUST_mpi_opt`, rebuild trust-LiMe against it);
   verify a small VEF case runs on GPU and matches CPU numerically.
2. (No-GPU middle ground) Build with `-kokkos_openmp` and benchmark
   thread-level speedup on the 4-thread host.
3. Port hot trust-LiMe operators to Kokkos: `Terme_Source_K_Magnetic`,
   `Terme_Source_Epsilon_Magnetic`, `Terme_Source_Induction_stretching`,
   `Terme_Source_Lorentz`, `Modele_turbulence_magnetique`,
   `Champ_Fonc_Eta_Turbulent`, `Champ_Fonc_Somme`, boundary kernels
   (`Paroi_conductrice/isolante_magnetique`) — replace raw loops with
   `TRUSTArray` views + `Kokkos::parallel_for`/`parallel_reduce`, add
   fences, respect SPMD/MPMD rules.
4. Move magnetic k-ε verification forms (currently PolyMAC_P0) to VEF where
   feasible, since VEF is the GPU-supported discretization; keep PolyMAC as
   CPU fallback.
5. Switch pressure/Poisson solves to GPU-native solvers (PETSc/AMGX or
   cuDSS) and validate convergence.
6. Profile with Nsight (host/device copies, unported code) and minimize
   data movement; add a GPU regression/benchmark case under
   `tests/GPU/`-style or `share/Validation/`.
7. Document the GPU build/run workflow for trust-LiMe (env, configure flags,
   `TRUST_ROOT` selection, compute capability).

**Constraints:** run GPU builds at night with low parallelism (`make -j1/-j2`);
C++20; numerical agreement CPU vs GPU required before any form is marked
validated; no auto-push. Hardware note: this host has no detected Nvidia
GPU/driver, so GPU work needs a GPU-capable machine.

## 5. How this page is maintained

- Reconcile this page with `docs/content/plans.md`, validation records and
  `AGENTS.md` whenever project status changes.
- A passing formatting check is not evidence that a validation computation
  passed; distinguish formatting, targeted runs and the full reportable sweep.
