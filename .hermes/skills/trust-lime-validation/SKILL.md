---
name: trust-lime-validation
description: "Author and run trust-LiMe validation forms and 3D twins."
version: 0.1.0
author: Hermes Agent
license: MIT
platforms: [linux]
metadata:
  hermes:
    tags: [trust-lime, validation, notebooks, 3d-twin]
    related_skills: [trust-lime-build-diagnostics, trust-lime-plan-management]
---

# trust-LiMe validation

## When to use

- Adding or modifying a validation form under `share/Validation/Rapports_automatiques/`.
- Creating a 3D twin of an existing 2D coupled/standalone form.
- Running or debugging a notebook/dataset.

## Creating a 3D twin (coupled forms)

1. Place the 3D dataset in the **same** form's `src/` as `<name>_3d.data` (do not create a separate `MagneticCoupled3D/` folder — merge into `MagneticCoupled/` instead, per "Merging validation into an existing notebook").
2. Start from the 2D dataset; change `dimension 2` -> `3`, add the third axis to `Pave`/borders, add `Polyedriser dom` after `Mailler` (PolyMAC needs polyhedra in 3D), set `nb_composantes 3`, and add the missing boundary faces (devant/derriere).
3. Keep the analytic solution z-independent where possible; probes at `z = 0.5` (mid-plane) reuse the 2D probe positions.
4. Add the form to `parallel_ok[]` in `.opencode/scripts/check-validation.sh` if it uses partition machinery, then run `check-validation.sh --all`.
5. Copy a simple 3D notebook (e.g. `ShercliffDuct3D`) and adapt probes; run it headless with the TRUST env sourced and `$exec` pointing at the trust-LiMe module.

**User preference:** merge 3D forms into the existing 2D form's folder and notebook rather than creating separate `*3D` folders. Concatenate the 3D notebook cells into the 2D notebook with a markdown header (see "Merging validation into an existing notebook").

## Restart persistence testing

- Pattern: a **pre-run** solves 0..t_half and saves a `.sauv` (TRUST saves `pb.sauv` automatically at end of run); the **restart case** resumes via `reprise binaire $sauv` with `tinit = t_half`, `tmax = t_final`; compare against a **continuous reference** 0..t_final. Use `run.addCaseFromTemplate(..., pre_run=pre_run)` with a `pre_run(d, n)` that copies the pre-run dataset, substitutes templates, and calls `case.run()`.
- The `.sauv` filename follows the pre-run case name: `<pre_case_name>_pb.sauv`. The restart case's `$sauv` placeholder must point to `./pre_run/<pre_case_name>_pb.sauv` (not the restart case name).
- TRUST restart is **not bit-exact** for k/ε fields: a ~1e-3 relative error appears in the first post-restart step and does not accumulate (observed: k_b 4.8e-4, ε_b 1.3e-4 at t_final after a 0→0.25→0.5 split). Uniform fields (B=1) are exact. Set assertions to 1e-2, not 1e-6, and document the residual.
- All unknowns related to the magnetic equations (`k_b`, `epsilon_b`, `magnetic_field`) are saved/restored; the magnetic field (uniform) matches to machine precision.

## Creating a 3D twin (standalone forms)

For a standalone form (not coupled), in addition to the coupled-form recipe:

1. Place the 3D dataset in the **same** form's `src/` as `<name>_3d.data` (do not create a separate `MagneticStandalone3D/` folder for a single notebook — merge into the existing notebook instead, per "Merging validation into an existing notebook").
2. Full-vector magnetic field: `nb_composantes 3` with `magnetic_field Champ_Fonc_xyz dom 3 <bx> <by> <bz>` (not scalar 1). Velocity becomes 3 components.
3. 3D divergence-free velocity and 3D curl production (`P_b = η_t |∇×b_A|²` with all three components).
4. Neumann BCs on all **six** faces (gauche/droite/bas/haut/devant/derriere) for every equation.
5. For **VEF** in 3D, no mesh conversion is needed (VEF handles hexahedra directly); `Trianguler` is 2D-only. For **PolyMAC** in 3D, `Polyedriser dom` is required right after `Mailler`.
6. Probes use 3 coordinates (`Points 1 x y z`); read effective coordinates from the `.son` header.
7. Keep the analytic solution simple (z-independent where possible) so the 3D case is a sanity check on the operators, not a full re-derivation of 2D analytics.

## Reading TRUST probes (`.son`)

- Use `SonFile(path, None)` (the single-arg form is not supported), then `son.getEntries()` and `son.getValues(entry)`.
- Probe files land in `src/` next to the dataset; copy them into `BUILD_DIRECTORY` after `runCases()` or read from `src/`.
- Effective probe coordinates come from the `# Temps ... x= ... y= ...` header; evaluate analytic solutions there, not at nominal coordinates.

## Dataset comment markers (parallel forms)

- `# BEGIN MESH #` is a closed comment on one line (MESH block stays active in nominal mode).
- `# BEGIN PARTITION` and `# BEGIN SCATTER` must stay **open** (no trailing `#`) so the block is commented out in nominal mode and only activated by `make_PAR.data`/`trust -partition`.
- Never bulk-close these markers with a global sed: it activates partitioning and can skip the solve.

## VEF template substitution (PolyMAC + VEF in one dataset)

To run the same form on both discretizations from one notebook, use `string.Template`
placeholders in the `.data` file and pass them via `run.addCaseFromTemplate(..., dic=...)`:

- `$discretization` -> `Polymac_p0` or `VEFPreP1B` (replaces `Polymac_p0 dis`).
- `$mesh_post` -> empty for PolyMAC 2D pave, or `Trianguler dom\nVerifierCoin dom { }` for VEF.
- `$post_processing` -> the full `Post_processing { ... }` block for PolyMAC, or empty for VEF.

**Why empty for VEF**: the `operateur_eqn` / `source operateur_eqn` post-processing block
segfaults on VEF (`Error check for missing "}"` / segfault at `Solve pb`). Disable
post-processing entirely for the VEF variant and skip the CONV/DIFF/SOURCE
assertions for that scheme.

- The poly case keeps the full `Post_processing` block (extract it to a helper file or
  embed as a triple-quoted string in the notebook; keep brace balance exact — the
  block must not include the closing `}` of `Read pb`).
- Pass the block via `dic={"post_processing": block_text}` for poly and `""` for vef.

## Validation status file


- `.opencode/validation_status.json` is versioned evidence: commit it every time it changes (validation run or `record-validation-status.py`). Never leave it modified-but-unstaged.
- Generated artifacts (`build/` dirs, `.son`/`.sons`/`.TU`/`.stop` files) under `share/Validation/Rapports_automatiques/` are gitignored and are deleted automatically by `run-validation-pdf.sh` after `trust-LiMe_validation.pdf` is produced. Do not commit them; do not hand-clean them mid-run.

## Citation of external sources

- **Cite external sources.** Any work derived from an outside paper, project, or
  third-party code must be duly cited (author, year, title,
  equation/figure/table) in the notebook, the data file header, and the
  relevant documentation page. This is of paramount importance; prefer
  primary sources.

## In-repo Hermes skills

- Project-local Hermes skills live in `./.hermes/skills/` (not `.opencode/skills/`).
- Gitignore exception required: add `!.hermes/skills/trust*` alongside the existing `.opencode/skills/trust*` rule, otherwise `trust*` ignores them.
- Trust the repo once with `hermes skills trust` so the skills load in sessions started inside it.

## Developing / inertial flow benchmarks

- A finite-duct case with a **plug (uniform) inlet** and an **open outlet carrying the Hartmann pressure profile** exercises `u·∇u ≠ 0` — the regime the fully-developed `HartmannTwoWay` case cannot (there `u·∇u = 0`).
- Inlet: `frontiere_ouverte_vitesse_imposee` with a uniform profile; outlet: `frontiere_ouverte_pression_imposee` with the Hartmann magnetic-pressure profile (`-0.5*(sinh(10*y)/742.099-y)^2`). Leave the inlet pressure floating — do not impose it.
- The steady state develops from plug toward Hartmann. The Hartmann reference centreline is `u_H = G h^2/(μ Ha^2)(1 - 1/cosh Ha)` with `G` from the mass flow `M = 2h U_plug`: `G = M μ Ha^2/(2 h^3 (1 - tanh(Ha)/Ha))`. Compare outlet probes to this reference.
- Mass flow is conserved, so the Hartmann centreline (~1.23 × plug velocity at Ha=5) is **higher** than the plug velocity: the flow **accelerates**, it does not decelerate. A benchmark asserting "outlet < inlet" would be physically wrong.
- Mesh-convergence: use `run.addCaseFromTemplate` with `$nx`/`$ny` placeholders over coarse/standard/fine; read effective probe positions from the `.son` header (`probe_xy`) because `grav` snaps to cell centres and positions shift with refinement.
- Assert **outlet velocity convergence** (std→fine diff below tolerance) rather than a fixed development ratio, which stays mesh-dependent.
- Tighter physical checks worth adding (see `MagneticCoupled/HartmannDeveloping`):
  - **Monotonic development**: centerline velocity must not decrease downstream (`u0 <= u1 <= ... <= u4` within solver noise, ~1e-3).
  - **Mass-flow conservation**: trapezoidally integrate the outlet profile (include wall zeros at y=±h/2) and compare to the imposed inlet mass flow (M = U_inlet * height); expect |err| < 5e-2.
  - **Centerline is the maximum** of the outlet profile (Hartmann-like shape).
  - **Hartmann reference** (u_H) and the development ratio are *diagnostics*, not hard asserts: the reference depends on mass flow and the ratio stays mesh-dependent.
  - **Profile symmetry** is a diagnostic only — `grav` snaps probes to cell centres, so effective y positions are asymmetric on coarse meshes (diffs halve with refinement); do not assert exact symmetry on coarse levels.
- **Full Hartmann profile shape comparison** (mass-flow-fitted, normalized by centerline) shows ~25–30% max error across the ladder; the profile is Hartmann-like (centerline maximum, boundary layers) but asymmetric from probe snapping and entry-flow effects. Treat as a diagnostic, not a pass/fail gate — the benchmark measures a developing flow, not a fully-developed one.
- **Time convergence**: with `tmax = 5` the outlet centerline drifts < 1e-6 over the last 5 steps (viscous time ~0.25, so 20× over-converged; the magnetic field's initial condition is already uniform/steady). If a developing-flow case shows drift > 1e-3, raise `tmax` or check the pressure BC — time is rarely the culprit.
- **Pressure-BC calibration caveat**: the outlet pressure constant (`742.099` = 10·cosh(5)) is calibrated for a specific mass flow. If the observed centerline exceeds the mass-flow-fitted Hartmann limit, the outlet pressure BC is over-driving the flow — recalibrate the constant to the imposed inlet mass flow (`M = U_inlet * height`) rather than changing the inlet.
- **Convection scheme**: the default `amont` (upwind) adds numerical diffusion that widens the profile; `muscl` (available in `Op_Conv_EF`) sharpens it. Switching scheme changes the development ratio, so pin the scheme in the dataset and report it.

## Conducting-wall BC (`paroi_conductrice_magnetique`)

- Only a scalar 2D smoke form exists (`MagneticStandalone/ConductingWall`); no 3D/vector/mixed (conducting+insulating) case, no `Normal_component_index` unit tests, and no post-processing with conducting walls.
- The BC is a perfect conductor: tangential `b = 0` (Dirichlet), normal component Neumann. `gradient_b_elements` uses `Normal_component_index` to pick the normal component; the correction operator (`corriger_divergence`) currently treats conducting walls as all-Neumann — keep these consistent when adding Dirichlet support.
- `Normal_component_index` is header-only (`src/Normal_component_index.h`) and directly unit-testable; add tests for scalar model (-1), 2D/3D vector normals, tie-breaking (lowest index), and invalid inputs.
- When validating, exercise mixed conducting/insulating walls and post-process `rotationnel_magnetique`/`courant_magnetique`/Lorentz force.

## TRUST kernel limitations (relevant when porting closures)

- `Pb_Induction` is **not supported on VEF** in the TRUST kernel: `Resoudre::interpreter` -> `objet("pb")` fails during `Solve pb` and the resulting `Process::exit` triggers a Kokkos teardown segfault (secondary). No VEF+Pb_Induction cases exist in the trust-code validation suite (only PolyMAC). Do not attempt a VEF port of an induction-only problem via `Pb_Induction`.
- For VEF coverage of magnetic-turbulence closures, use `Pb_MHD`/`Pb_MHD_Turbulent` (TRUST already supports these on VEF — `TurbulentDiffusion` is the reference). Adapt the manufactured case to `Pb_MHD_Turbulent` to exercise `modele_turbulence_magnetique` + VEF variants without touching the kernel.
- Kernel-level registration of `Pb_Induction` for VEF would be a substantial, risky undertaking outside module scope; prefer the `Pb_MHD_Turbulent` alternative.
- **`Pb_MHD` + frozen NS does NOT work as a `Pb_Induction` substitute on VEF**: marking `Navier_Stokes_standard` as `equation_non_resolue` still leaves a pressure unknown whose Poisson matrix is singular (constant pressure null space) with zero/prescribed velocity -> `KSP_DIVERGED_NANORINF` / segfault at `Solve pb`. `Pb_Induction` exists precisely to avoid the pressure system. Do not use this workaround.
- **VEF port status of `MagneticTurbulence`**: the code-side port is complete (VEF variants of `Champ_Fonc_Eta_Turbulent`/`Champ_Fonc_Somme` exist and `Modele_turbulence_magnetique`/`Convection_Diffusion_Magnetic_field` dispatch via `sub_type(Domaine_VEF)`). The **frozen manufactured case** now runs on VEF inside the *same* notebook via template substitution (`$discretization`/`$mesh_post`/`$post_processing` placeholders; `coarse_poly` + `coarse_vef`, all cases pass). The **solved cases** (decay/transport/energy) remain PolyMAC-only: `Pb_Induction` is not supported on VEF for solving, and the `Pb_MHD`+frozen-NS workaround diverges (`KSP_DIVERGED_NANORINF` from the pressure null space with zero velocity). Do not attempt the workaround; keep solved cases PolyMAC-only.

## GPU / Kokkos (hardware feasibility)

- **This workstation's GPU is a consumer Radeon RX 7600/7700S (Navi 33, gfx1102/RDNA3)** and cannot be used for TRUST computations: TRUST's ROCm path supports only MI250X (gfx90a) and MI300A (gfx942), no `hipcc` compiler is installed, and `clinfo` reports 0 OpenCL platforms. The GPU is display-only.
- **TRUST GPU acceleration is experimental** and only tested on HPC AMD nodes (MI250X/MI300A) or NVIDIA GPUs with CUDA. Do not attempt a GPU build on consumer hardware.
- **Mark the GPU/Kokkos backlog task as not feasible** on this workstation; it requires an HPC node or NVIDIA GPU.

## Meshes and Git LFS

- TRUST stores its meshes in the **same GitLab repository** (`cea-trust-platform/trust/trust-code`), tracked via **Git LFS** (`.gitattributes` filters `*.med`, `*.geom`, `*.msh`, `*.gz`, `*.cgns`, etc.). They are not in a separate repo.
- trust-LiMe has **no committed mesh files** yet (cases generate meshes inline or via SALOME scripts). The repo's `.gitattributes` (mirroring trust-code's) enables LFS for future heavy VKS meshes; `git lfs` is installed and the filter is configured.
- **SALOME meshes require system dependencies**: the bundled SALOME (e.g. `SALOME-9.16.0-native-UB24.04`) segfaults in `_runSession` if runtime libs are missing (metis, openblas, numpy/scipy, etc.). Run `$SALOME/sat/sat config SALOME-9.16.0-native --check_system` to list missing packages and install them (needs sudo). Without SALOME, `gmsh` cannot substitute for spherical-shell geometry because this build lacks OpenCASCADE (no sphere booleans). Prefer the box-based inline `Mailler` cases (`vks_lite`, `vks_sodium`) when SALOME is unavailable.
- **TRUST Read_MED for tetra MED meshes:** use `convertAllToPoly` instead of `Polyedriser` (Polyedriser cannot convert tetrahedra: "We do not yet know how to Polyedriser Tetraedres"). Specify `mesh <name>` when the MED file has multiple entries (groups appended as separate meshes). Without these, TRUST fails to read the mesh or reports "no boundary detected".
- **GitHub LFS free tier is 1 GB storage + 1 GB/month bandwidth; large VKS meshes may exceed it. Consider a GitLab LFS endpoint or self-hosted LFS server before pushing big meshes.**

## Generating a cylindrical VKS mesh (Monchaux et al. geometry)

The real VKS experiment is **cylindrical** (not spherical): inner cylinder
r=0.206 m, h=0.524 m, impellers at z=±0.1855 m (radius 0.1545 m). The
in-repo script `salome_vks_cylindrical_9x.py` (SALOME 9.x, 3 resolutions:
coarse/medium/fine) produces groups `wall_cyl`, `wall_top`, `wall_bottom`.

- **Do NOT partition the volume by impeller disks.** `geom.MakePartition([cyl], [disk_top, disk_bot])` creates mesh topology that fails TRUST discretization (SIGFPE during `NettoieNoeuds`). The partitioned mesh has 5-node cells and invalid topology regardless of mesh size or algorithm (Tetrahedron vs AutomaticTetrahedralization, PolyMAC vs VEF).
- **Working approach:** mesh the unpartitioned cylinder with `AutomaticTetrahedralization()`, then create groups on boundary faces identified by bounding box (wall_cyl = side face, wall_top/bottom = end caps). Impeller disk surfaces (`impeller_top`/`impeller_bottom`) are **not** included.
- **`GroupOnGeom` on original shapes returns 0** after boolean ops (MakeCut/MakePartition consumes them). Identify faces by `geom.BoundingBox(shape)` and create groups on individual faces, then `geom.MakeCompound` to merge before `GroupOnGeom` (avoids duplicate group names like `GR_1_wall_outer`).
- **Group export:** `mesh.ExportMED(file, False, 32, True)` (main) then `mesh.ExportMED(file, False, 32, False, meshPart=group)` for each group — merges groups into the main mesh entry (verified via `GetMeshGroupsNames`).
- **TRUST Read_MED:** use `convertAllToPoly` (not `Polyedriser`), specify `mesh vks_cyl`. Verified working with all three resolutions.
- **Impeller disks deferred:** partitioning creates invalid mesh. Alternative: model impellers as `paroi_defilante` on `wall_top`/`wall_bottom` end caps (proxy). Full impeller disk surfaces need a different approach (SALOME GUI with 8.x script, or post-processing).
- **Geometry orientation:** cylinder axis is Z (vertical, Z-up), centered at origin (z from -0.262 to +0.262). Matches `vks_lite.data` convention (flow along Z).

## Generating a spherical-shell mesh with SALOME 9.x

The in-repo script `share/Validation/Rapports_automatiques/VKS_experiment/VKSLite/geometry/salome_vks_spherical_shell.py` targets SALOME 8.x APIs. Under SALOME 9.x (e.g. `SALOME-9.16.0-native-UB24.04`) adapt it:

- Drop the top-level `import GEOM` / `import MESH` / `import SMESH` (unavailable); keep `from salome.geom import geomBuilder` and `from salome.smesh import smeshBuilder`. `import SMESH` (for `SMESH.FACE`) is available inside the session.
- `geom = geomBuilder.New()`; `smesh_gen = smeshBuilder.New()` (not `meshBuilder.New()`).
- `mesh_shell = smesh_gen.Mesh(shell)` (not `CreateMesh(shell, name)`).
- Geometry: `geom.ShapeType["FACE"]` instead of `geom.FACE`.
- Algorithms: `mesh_shell.Segment().MaxSize(...)` (or `NumberOfSegments`), `mesh_shell.Triangle().MaxElementArea(...)`, then **`mesh.AutomaticTetrahedralization()`** (do NOT use `mesh.Tetrahedron()` — the NETGEN default can produce 5-node pyramid cells; `AutomaticTetrahedralization` produces clean tetra). Then `mesh.Compute()`.
- Boundary-face classification: `geom.GetPosition` returns the **center of mass** (origin for closed spherical surfaces), so it cannot distinguish outer vs inner sphere. Use `geom.BoundingBox(shape)` and compare the extent (`bb[1]` = max x) to `r_outer`/`r_inner`.
- Groups: `mesh_shell.GroupOnGeom(shape, name, SMESH.FACE)` still works.
- Export: `mesh_shell.ExportMED(file, False, 32, True)` for MED 3.2.x, then append each group with `mesh_shell.ExportMED(file, False, 32, False, meshPart=group)` so the groups appear in the main mesh entry (verified via `GetMeshGroupsNames`). Run headless: `SALOME_MODULES="GEOM:SMESH" xvfb-run -a $SALOME/salome shell script.py` (no display required).
- Missing runtime libs (boost 1.83.0, libtbb) can be satisfied by symlinking from a compatible local boost build into the module's `lib/salome/` directory; check with `$SALOME/sat/sat config SALOME-9.16.0-native --check_system`.

**Pitfalls (observed when adapting the 8.x script to 9.x):**

- **Mesh topology is not guaranteed to be clean tetra with the default algorithm.** `mesh.Tetrahedron()` (NETGEN default) can produce cells with 5 nodes (pyramids) rather than 4 (tetra), and boundary faces with 4 nodes (quads). Use `mesh.AutomaticTetrahedralization()` instead, which produces clean tetra (verified: `MEDCouplingUMesh` reports 4 nodes/cell). Verify with `MEDCouplingUMesh` (node count per cell) before trusting the mesh.
- **Group export in 9.x does merge into the main mesh.** Calling `mesh.ExportMED(file, False, 32, True)` (main mesh) followed by `mesh.ExportMED(file, False, 32, False, meshPart=group)` for each group **does** add the group as a named group inside the main mesh entry (verified via `GetMeshGroupsNames`). The earlier "separate mesh entries" pitfall was a misread: the append creates both a separate entry AND a group in the main mesh; TRUST reads the groups from the main entry. Keep the append calls; do not omit them.
- **`getLevelM1Mesh()` faces are not the boundary faces** of a non-tetra mesh; classifying faces by centroid on level-1 (as in a quick post-process) silently misses the outer boundary. Identify boundary faces from the volume-cell connectivity instead (faces appearing in exactly one cell).
- **SALOME 9.x adaptation is now TRUST-validated.** The in-repo script `salome_vks_spherical_shell_9x.py` targets 9.x and has been verified: TRUST `Read_MED` reads the mesh and resolves named boundary groups (wall_outer, wall_inner). Use `convertAllToPoly` (not `Polyedriser`) and specify `mesh <name>` in the `.data` file. The box-based inline `Mailler` cases (`vks_lite`, `vks_sodium`) remain a reliable fallback when SALOME is unavailable.

## Gauss-law projection (`correction_divergence`)

- Dirichlet support is implemented as a **constrained projection**: Dirichlet boundary faces are excluded from the Jacobian `J` (their trace is a constant) and their prescribed `b0` enters `D(b)` as a fixed term, so prescribed boundary values are never overwritten. See `docs/content/plans/gauss_law_dirichlet.md`.
- `mettre_a_jour()` must call `build_bc_trace_tables(temps)` before `corriger_divergence()` so time-dependent Dirichlet CLs use current prescribed values; `assembler_correction()` caches `L` (type-only, no value refresh needed).
- `gradient_b_elements()` detects Dirichlet CLs independently (`is_dirichlet_face`) and uses prescribed values for the reported divergence; keep it consistent with `bc_trace`.
- Kernel unit test: mark boundary faces as `BC_DIRICHLET` with prescribed values; verify (a) divergence uses prescribed trace, (b) net flux uses prescribed value, (c) Jacobian is reduced, (d) correction is finite.
- **Form-level (Dirichlet wall):** prescribed Dirichlet fluxes must be **compatible** (net boundary flux ≈ 0) or the run aborts with `Gauss law violated by boundary conditions`. Opposite walls need the same normal component (e.g. bottom and top both `b=(0,1)`), not opposite signs.
- **Dirichlet CL syntax:** use `temperature_imposee_paroi` (thermal CL reused because `domaine_application() == "Thermique"`) with a `Champ_Front_Uniforme` carrying `nb_composantes` values, e.g. `bas temperature_imposee_paroi Champ_Front_Uniforme 2 0. 1.`. The CL is a `Dirichlet` subclass, so `build_bc_trace_tables` detects it automatically.
- **`divergence_magnetique` post-treatment field requires `nb_composantes == dimension`** (full vector field); with fewer components `creer_champ` calls `Process::exit()`. Use `nb_composantes 2` in 2D / `3` in 3D when probing divergence.
- **PolyMAC supports only 1-component magnetic fields in 3D.** `Convection_Diffusion_Magnetic_field` on a PolyMAC mesh (`Polymac_p0`) rejects `nb_composantes 3` (\"only one transported component is supported by the current PolyMAC diffusion operator\"). For 3D vector MHD (3 components), use VEF discretization (hexahedral mesh) or accept a scalar proxy. The `nb_composantes` keyword must be omitted (defaults to 1) for PolyMAC; setting it to 3 aborts the run. Full 3-component magnetic field requires VEF + `VEFPreP1B`/`PolyMAC` with `nb_composantes 3` (but PolyMAC's diffusion operator is scalar-only).

## MED-based forms (unstructured meshes)

When a form uses a `.med` mesh (e.g. VKS cylindrical), the trustutils flow
copies `src/*` (including the `.med`) into `build/`, but `make_PAR.data` runs
the decomposition in `build/<level>/` (e.g. `build/coarse/`). The mesh file
is therefore not on the decomposition run's search path.

- **Use a relative path that resolves from `build/<level>/`**: `file ../vks_cyl_coarse.med`
  (points at `build/vks_cyl_coarse.med`). The single-rank `src/` run then needs the
  `.med` next to the `.data`; keep a `vks_cyl_coarse.med` copy in `src/` or run
  only through the notebook for MED-based forms.
- **Boundary groups may not propagate to the scattered (PAR) mesh**: a 2-rank run
  can fail with `We have not found a boundary with name <group>` even though the
  single-rank run and `GetMeshGroupsNames` show the group. Fall back to a
  single-rank run (`nbProcs=1`) for MED-based forms, or verify the PAR run
  separately before relying on it.
- **`force_lorentz` coupling diverges on VEF tetra meshes with `paroi_defilante`
  impeller BCs** (`KSP_DIVERGED_NANORINF` / segfault), even with a zero magnetic
  seed; it is stable with `paroi_fixe` walls or with the seed set to zero. Treat
  `force_lorentz` + `paroi_defilante` as unsupported on these meshes until the
  spurious-current issue is fixed; keep the coupling disabled (commented) and
  document the blocker, or use `paroi_fixe` walls for coupled runs.
- **Do not delete generated artifacts** (`build/`, logs, `.med`-derived outputs)
  for VKS/MED forms — the user inspects them manually.

## Merging validation into an existing notebook

- Prefer folding a new validation into an existing sibling notebook over creating another `<FormName>.ipynb`. Extra notebooks proliferate; a single notebook per physical topic keeps the reportable suite navigable.
- Steps:
  1. Move the new case's `.data` into the target form's `src/` (e.g. `GaussLaw/src/gauss_law_dirichlet.data`).
  2. Append cells to the target `.ipynb` via JSON (never hand-edit markdown): a `run.reset()`, an `addCaseFromTemplate(...)` + `runCases()`, a probe-read/assert cell, and a `run.tablePerf()`. Build the cell list in Python and `json.dump` with `indent=1`.
  3. Validate by running the appended section headless: create a temporary notebook containing only those cells plus the `TRUST_parameters`/`reset` setup, place it in the form dir, run `jupyter nbconvert --to notebook --execute` with `env_trust_LiMe.sh`, `env_for_python.sh`, and `venv_trust/bin/activate` sourced, then delete the temp notebook.
  4. Remove the now-redundant standalone form directory (notebook + build + checkpoints) but keep its `src/*.data` (moved, not deleted).
  5. Drop the stale entry from `.opencode/validation_status.json` (the form no longer exists).
- Keep the assertion that captures the actual physics being validated; do not weaken it to make the merge pass.
- `run.reset()` in the appended section is safe because prior sections have already asserted their results; it only clears case state for the new run.

## Mesh generation with SALOME 9.x

When a validation form needs an unstructured mesh (e.g. spherical/cylindrical VKS geometry) and SALOME is available:

- **Run headless**: `SALOME_MODULES="GEOM:SMESH" xvfb-run -a $SALOME/salome shell script.py`. No display needed.
- **TRUST Read_MED for tetra meshes**: use `convertAllToPoly` (not `Polyedriser` — Polyedriser cannot convert tetra: "We do not yet know how to Polyedriser Tetraedres"). Specify `mesh <name>` when the MED file has multiple entries (groups appended as separate meshes).
- **SALOME 9.x API differences from 8.x**: `smeshBuilder.New()` (not `meshBuilder.New()`), `mesh = smesh_gen.Mesh(shape)` (not `CreateMesh`), `geom.ShapeType["FACE"]` (not `geom.FACE`), `mesh.AutomaticTetrahedralization()` (not `mesh.Tetrahedron()` — the NETGEN default can produce 5-node pyramid cells; AutomaticTetrahedralization produces clean tetra). Verify cell node count with MEDCoupling before trusting the mesh.
- **Boundary-face identification after boolean ops**: `GroupOnGeom` on the original input shapes returns 0 (they are consumed by MakeCut/MakePartition). Identify faces by `geom.BoundingBox(shape)` and create groups on individual faces, then `geom.MakeCompound` to merge before `GroupOnGeom` (avoids duplicate names like `GR_1_wall_outer`).
- **Group export**: `mesh.ExportMED(file, False, 32, True)` (main) then `mesh.ExportMED(file, False, 32, False, meshPart=group)` for each group — merges groups into the main mesh entry (verified via `GetMeshGroupsNames`).
- **Partition creates invalid mesh**: `geom.MakePartition([vol], [surf1, surf2])` creates internal faces that fail TRUST discretization (SIGFPE during NettoieNoeuds). Do not partition; mesh the unpartitioned volume and identify boundary faces by bounding box.
- **Partition creates invalid mesh for TRUST.** `geom.MakePartition([vol], [surf1, surf2])` creates internal faces that fail TRUST discretization (SIGFPE during `NettoieNoeuds`). The partitioned mesh has 5-node cells and invalid topology regardless of mesh size or algorithm. Do not partition; mesh the unpartitioned volume and identify boundary faces by bounding box. Impeller disk surfaces require a different approach (SALOME GUI with 8.x, or post-processing).
- **Geometry orientation:** keep the cylinder axis along Z (vertical, Z-up) to match `vks_lite.data` convention.
- **Missing runtime libs**: SALOME segfaults if system deps are missing. Run `$SALOME/sat/sat config SALOME-9.16.0-native --check_system` to list them. Missing boost (1.83.0) / libtbb can be satisfied by symlinking from a compatible local build into the module's `lib/salome/` directory.

## Pitfalls
- In 3D, `Polyedriser dom` must come right after `Mailler` (PolyMAC needs polyhedra).
- A 3D form needs all six boundary faces defined in BOTH the Navier-Stokes and magnetic blocks; missing front/back faces break meshing or the solve.
- `check-validation.sh` defaults to git-tracked files; use `--all` to scan new untracked forms. Re-run it after editing `parallel_ok[]`.
- TRUST `.data` comments must be **closed on the same line** (`# ... #`): an opening `#` with no matching close makes the parser treat the following words as keywords (error: `Error while reading 'X,'`). This includes header/comment blocks — safest is to keep comments minimal or fully closed.
- Boundary-condition fields go on the **next line**, not inline: `inlet frontiere_ouverte_vitesse_imposee` then `Champ_front_Uniforme 2 1. 0.` on the following line. Case matters (`Champ_front_Uniforme`, not `Champ_front_uniforme`).
- A new form's notebook must be **valid JSON** (`.ipynb`). Build it with `json.dump` rather than hand-writing markdown; run it headless with `jupyter nbconvert --to notebook --execute` after sourcing `env_trust_LiMe.sh`, `env_for_python.sh`, and `venv_trust/bin/activate`.
- Heavy builds (sanitizer/full-kernel) follow the RAM guard in `AGENTS.md` and `.opencode/scripts/check-heavy-build.sh`.
- For VEF tetra meshes, `force_lorentz` + `paroi_defilante` impeller BCs diverge (KSP_DIVERGED_NANORINF); use `paroi_fixe` walls or disable the coupling, and keep `dt <= 0.02` for coupled runs.
- On VEF meshes, the `operateur_eqn` / `source operateur_eqn` post-processing block (used by `MagneticTurbulence` for CONV/DIFF/SOURCE probes) segfaults at `Solve pb`. Disable post-processing entirely for VEF variants (pass an empty `$post_processing` placeholder) and skip the manufactured-term assertions for that scheme.
- For VEF coupled (force_lorentz) runs, keep `dt_max <= 0.02`; larger steps (0.05) diverge even on fine meshes.
- The nightly backlog job runs each task in its own worktree under `trust-LiMe-worktrees/`; changes are NOT in the main tree. Validate by inspecting the worktree paths in `nightly-backlog-state.json` (`worktrees`), then remove the worktree after merge.
- When a task directly follows the previous one, the job reuses the same worktree (entry lists `tasks: [title1, title2, ...]`). Validate each task before the worktree is cleared; do not treat the worktree as validated until all tasks in it are validated.
- A dataset using `$nx`/`$ny`/`$nz` placeholders is a template: the notebook must call `run.addCaseFromTemplate(..., dic={...})`. Calling `run.addCase(".", ...)` leaves the placeholders in the generated `DEC_`/`PAR_` files and fails with `Error converting a string to type int: $nx`.
- **Restart is not bit-exact for extra equations.** `reprise binaire $sauv` restores the primary fields, but `k_b`/`epsilon_b` (and their internal state) lose ~1e-3 relative precision on the first post-restart step (a one-step transient that does not accumulate; uniform fields like `b` are exact). Do not assert bit-identity for restarted magnetic-turbulence cases; use a 1e-2 tolerance and document the observed error. Pin the sauv filename to the actual pre-run case name (`<pre_case>_pb.sauv`), not the restart case name.
- In 3D, keep the analytic solution z-independent where possible and place probes at the mid-plane (`z = 0.0` or `0.5` matching the mesh); always read effective coordinates from the `.son` header rather than nominal ones.
- `SonFile(path)` (single argument) is not supported; use `SonFile(path, None)` then `getEntries()`/`getValues()`.


**Exception:** if the only change is the `unit_tests` block or the `updated`
timestamp produced by the pre-commit hook (unit-test results), do **not**
commit it — revert with `git checkout -- .opencode/validation_status.json`
(or `git restore`). Those are hook noise, not a validation-state change worth
versioning. Only commit validation_status.json when a real validation run,
`record-validation-status.py` (non-unit), or a form result changed it.
