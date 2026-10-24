# VKS (Von Kármán Sodium) Simulation Plan — trust-LiMe baltik

**Status (2026-10-24): VKS simulation not started; enabling foundations are
partly implemented.** Effective magnetic diffusivity can be built from fields
or hydraulic k-ε, and the 3D `VKSTurbulent` integration case exercises two-way
VEF coupling. An opt-in cell-centred Gauss-law projection and Picard coupling
are also implemented, but only low-Rm validation exists. No spherical VKS
geometry, rotating impellers, high-Rm threshold or saturated dynamo has been
validated. This remains a design plan, not a runnable VKS case.

## 1. Overview

The VKS experiment (CEA-DAM / CNRS) studies the dynamo effect in turbulent
liquid sodium driven by two counter-rotating impellers in a spherical shell.
Simulating it requires:

- **High-Rm MHD** (Rm ~ 100–1000): the induced field is O(1), not a
  perturbation. The current trust-LiMe mode is **low-Rm** (Rm ≪ 1, lagged
  staggered scheme). The VKS regime is fundamentally beyond the current
  validated range.
- **Turbulence modelling**: Re ~ 10⁵–10⁶, requiring k-ε or LES closures.
- **Spherical geometry**: unstructured polyhedral mesh with impeller surfaces.
- **Moving boundaries**: impeller rotation (rotating mesh or body force).
- **Sodium properties**: temperature-dependent conductivity, density, viscosity.

## 2. Governing equations (what the data file must encode)

### 2.1 Induction equation (trust-LiMe, extended)
```
∂b/∂t + (u·∇)b = (b·∇)u + η Δb,   ∇·b = 0
```
with **turbulent magnetic diffusivity** η_t (EDM) added:
```
η_eff = η + η_t(k, ε, b)
```
The equation accepts molecular η plus an effective η_t supplied as a field,
computed from magnetic k/ε fields, or obtained from the hydraulic k-ε model.
The magnetic-fluctuation transport equations and a calibrated VKS EDM closure
remain deferred (see `PLAN_induction_turbulence.md` and `END_GOAL.md` §2.2).

### 2.2 Momentum equation (TRUST Navier-Stokes)
```
ρ(∂u/∂t + u·∇u) = −∇p + μ Δu + J×B + F_impeller
```
with turbulence closure (k-ε or LES subgrid model).

### 2.3 Incompressibility
```
∇·u = 0
```

### 2.4 Dimensionless parameters (Tassone et al. 2024)
| Symbol | Definition | VKS typical |
|--------|-----------|-------------|
| Rm = μ₀ σ v₀ L | Magnetic Reynolds | 100–1000 |
| Re = v₀ L / ν | Reynolds | 10⁵–10⁶ |
| Ha = B₀ L √(σ/ρν) | Hartmann | O(1)–O(10) |
| Pm = ν/η | Magnetic Prandtl | ~10⁻⁵ (sodium) |
| N = σ B² L / (ρ U) | Interaction | O(1) |
| Ro = U / (2Ω L) | Rossby | O(1) |

## 3. Data file structure (TRUST .data syntax)

A VKS data file follows the standard TRUST block structure. Below is the
skeleton with every input the file needs.

### 3.1 Header and dimension

```
# VKS simulation: two counter-rotating impellers in spherical shell #
dimension 3
Pb_MHD pb
Domaine dom
```

### 3.2 Mesh (spherical shell + impellers)

```
# BEGIN MESH #
# Unstructured polyhedral mesh of the spherical shell with impeller surfaces.
# Generated externally (Gmsh/Salome) and read via Read_MED, or inline Mailler
# for simplified geometries.
Read_MED {
    domain dom
    file vks_shell.med          # .med file with impeller surfaces tagged
}
Polyedriser dom                 # PolyMAC_MPFA needs polyhedra in 3D
# END MESH #
```

**Inputs needed:**
- `vks_shell.med` — mesh file with:
  - Domain: spherical shell (inner radius r_i, outer radius r_o)
  - Boundary groups: `impeller_top`, `impeller_bottom`, `wall_outer`,
    `wall_inner` (if inner sphere present)
  - Impeller surfaces tagged for rotating boundary conditions
- Mesh resolution: ~10⁶–10⁷ cells for DNS/LES; coarser for RANS

### 3.3 Partition (parallel)

```
# BEGIN PARTITION #
Partition dom
{
    Partition_tool metis { nb_parts $NPROCS }
    Larg_joint 2
    zones_name DOM
}
End
END PARTITION #
# BEGIN SCATTER #
Scatter DOM.Zones dom
END SCATTER #
```

**Inputs needed:**
- `$NPROCS` — number of MPI ranks (VKS needs hundreds to thousands)
- `metis` partition tool (available in TRUST)

### 3.4 Time scheme

```
Schema_Euler_Implicite sch
Read sch
{
    tinit 0.
    tmax $TMAX                    # total simulation time (seconds)
    dt_max $DT_MAX                # max time step
    facsec 1e6
    facsec_max 1e6
    solveur implicite { solveur petsc cli { } }
    # For turbulence: may need smaller dt for LES
}
```

**Inputs needed:**
- `$TMAX` — total simulation time (physical seconds)
- `$DT_MAX` — max time step (typically 1e-3 to 1e-2 s for LES)

### 3.5 Fluid (sodium) properties

```
fluide_incompressible
{
    mu      Champ_Uniforme 1 $MU       # dynamic viscosity (Pa·s)
    rho     Champ_Uniforme 1 $RHO      # density (kg/m³)
    lambda  Champ_Uniforme 1 $LAMBDA   # thermal conductivity (W/m/K)
    Cp      Champ_Uniforme 1 $CP       # specific heat (J/kg/K)
    beta_th Champ_Uniforme 1 $BETA     # thermal expansion (1/K)
}
```

**Sodium properties (at ~120°C):**
| Property | Symbol | Value | Unit |
|----------|--------|-------|------|
| Density | ρ | 929 | kg/m³ |
| Dynamic viscosity | μ | 6.9×10⁻⁴ | Pa·s |
| Kinematic viscosity | ν | 7.4×10⁻⁷ | m²/s |
| Electrical conductivity | σ | 1.03×10⁷ | S/m |
| Thermal conductivity | λ | 85 | W/m/K |
| Specific heat | C_p | 1380 | J/kg/K |
| Magnetic diffusivity | η = 1/(μ₀σ) | 0.078 | m²/s |
| Magnetic Prandtl | Pm = ν/η | ~10⁻⁵ | — |

**Inputs needed:** all of the above as `Champ_Uniforme` or temperature-dependent
functions (`Champ_Fonc_xyz`).

### 3.6 Magnetic equation (trust-LiMe)

```
Convection_Diffusion_Magnetic_field
{
    diffusivite_magnetique Champ_Uniforme 1 $ETA   # = 1/(mu0*sigma)
    vitesse_hydraulique                          # two-way coupling: NS velocity

    diffusion  { }
    convection { amont }

    sources
    {
        induction_stretching { }                 # M1 source
        # turbulent_EDM { }                      # deferred: turbulent EDM
    }

    boundary_conditions
    {
        # insulating walls: zero normal flux #
        wall_outer   paroi_isolante_magnetique
        wall_inner   paroi_isolante_magnetique
        # impeller surfaces: may need conducting BC (Phase B.2) #
        impeller_top     paroi_isolante_magnetique      # conductrice deferred (Phase B.2)
        impeller_bottom  paroi_isolante_magnetique      # conductrice deferred (Phase B.2)
    }
    initial_conditions { magnetic_field Champ_Uniforme 3 $BX $BY $BZ }
}
```

**Inputs needed:**
- `$ETA` — magnetic diffusivity = 1/(μ₀ σ) ≈ 0.078 m²/s
- `$BX, $BY, $BZ` — initial seed field (typically small, e.g., 1e-4 T)
- BC keyword: `paroi_isolante_magnetique` (Phase B.1, available) or
  `paroi_conductrice_magnetique` (Phase B.2, deferred)

### 3.7 Momentum equation (TRUST Navier-Stokes)

```
Navier_Stokes_standard
{
    solveur_pression Petsc Cholesky { }
    diffusion  { }
    convection { amont }

    # Turbulence model (k-epsilon or LES) #
    # Option 1: k-epsilon
    # turbulence k_epsilon { ... }
    # Option 2: LES
    # turbulence les { modele smagorinsky { Cs 0.1 } }

    boundary_conditions
    {
        # impeller rotation: moving wall or body force #
        impeller_top     paroi_defilante Champ_Front_Fonc_xyz 3 0. $V_top 0.
        impeller_bottom  paroi_defilante Champ_Front_Fonc_xyz 3 0. 0. $V_bot
        wall_outer       paroi_fixe
        wall_inner       paroi_fixe
    }
    initial_conditions
    {
        vitesse Champ_Uniforme 3 0. 0. 0.
    }
    sources
    {
        force_lorentz { }                        # J x B / rho
        # gravite { ... }                        # if buoyancy
    }
}
```

**Inputs needed:**
- Impeller velocities: `$V_top`, `$V_bot` (m/s, up to several m/s)
- Turbulence model choice and parameters (k-ε coefficients, LES Cs)
- `solveur_pression`: Cholesky (direct) or PETSc (iterative, for large meshes)

### 3.8 Impeller rotation

Two approaches in TRUST:

**Option A: Rotating wall (paroi_defilante)**
```
impeller_top paroi_defilante Champ_Front_Fonc_xyz 3 0. $V_top 0.
```
The wall velocity is imposed as a Dirichlet condition on the impeller surface.

**Option B: Body force (source term)**
```
sources { force_rotative { ... } }
```
A volumetric body force drives the rotation (less physical but simpler).

**Inputs needed:**
- Impeller geometry (radius, blade count, pitch)
- Rotation speed (rad/s or m/s at tip)
- Direction (co-rotation or counter-rotation)

### 3.9 Post-processing (probes, fields)

```
Postraitement
{
    Sondes
    {
        # magnetic probes (induction coils) at VKS measurement positions #
        sonde_bx_1 grav magnetic_field0 Periode $DT_PROBE Points 1 $X1 $Y1 $Z1
        sonde_bx_2 grav magnetic_field0 Periode $DT_PROBE Points 1 $X2 $Y2 $Z2
        # ... (array of probes)
        sonde_u  grav vitesse Periode $DT_PROBE Points 1 $X $Y $Z
        sonde_j  grav courant_magnetique Periode $DT_PROBE Points 1 $X $Y $Z
    }
    Champs
    {
        magnetic_field elem
        vitesse elem
        courant_magnetique elem
        # for post-treatment: energy, Poynting, helicity #
    }
    format lml
}
```

**Inputs needed:**
- Probe positions ($X, $Y, $Z) from VKS experimental measurement array
- Probe period ($DT_PROBE) — typically 1e-3 to 1e-2 s
- Fields to output (for post-processing in ParaView/VisIt)

### 3.10 Sodium physical properties (temperature-dependent)

For full VKS, sodium properties vary with temperature. TRUST supports
temperature-dependent properties via `Champ_Fonc_Fonction` / `Champ_Fonc_Tabule`
referencing the solved temperature field (`pb temperature`), and (for the fluid)
via the incompressible Boussinesq path (mu, lambda T-dependent; rho, Cp,
beta_th uniform). For trust-LiMe's own magnetic diffusivity eta = 1/(mu0*sigma),
which is not a TRUST fluid property, use the same mechanism:

```
diffusivite_magnetique Champ_Fonc_Fonction pb temperature 1 \
    1./(mu0 * (sigma0 * (1 + alpha * (val - T0))))
```

or a `Champ_Fonc_Tabule { pb temperature } 1 { ... }` for tabulated sigma(T).

**Inputs needed:**
- Reference temperature (T0, typically 120 C)
- Temperature coefficient (alpha) for sigma(T), or a tabulated sigma(T)
- Operating temperature range
- Magnetic permeability mu0 (default 1) and reference conductivity sigma0

Note: the earlier `Champ_Fonc_xyz` static-spatial form is NOT temperature-dependent
during the solve; use `Champ_Fonc_Fonction`/`Tabule` referencing `pb temperature`.

## 4. What trust-LiMe currently provides vs. what VKS needs

| Capability | trust-LiMe status | VKS need |
|-----------|-------------------|----------|
| Induction equation (M0/M1) | ✅ Done | ✅ (base) |
| Lorentz force (`force_lorentz`) | ✅ Done for PolyMAC and VEF (low-Rm) | ✅ (but needs high-Rm validation) |
| Two-way coupling (`vitesse_hydraulique`) | ✅ Done; opt-in Picard coupling is available | ✅ (high-Rm stability still unvalidated) |
| Post-treatment fields (energy, Poynting, helicity, J, tension) | ✅ Done | ✅ |
| Effective turbulent magnetic diffusivity | ✅ From prescribed fields or hydraulic/magnetic k-ε fields; `VKSTurbulent` exercises the hydraulic path | ⚠️ Magnetic fluctuation transport and calibrated closure needed |
| High-Rm regime (Rm ~ 100-1000) | ❌ Not validated | ❌ (critical for VKS) |
| Turbulence models (k-ε, LES) | ⚠️ Hydraulic k-ε can feed η_t; full turbulent MHD/LES not implemented | ❌ (critical for VKS) |
| Spherical geometry | ❌ Not tested | ❌ (needed) |
| Impeller rotation BCs | ✅ Available (`Paroi_defilante`); exercised in `SmoothDiskMHD` (smooth disks, no blades) | ✅ (smooth disks; blade-resolving deferred) |
| Smooth rotating disks (modelized propellers) | ✅ `SmoothDiskMHD` validation case (3D, 2-rank, laminar, Rm~1, runs to completion) | ✅ (smoke; high-Rm not validated) |
| Temperature-dependent properties | ⚠️ Fluid side (mu, lambda) supported by TRUST (Boussinesq path); eta = 1/(mu0*sigma) supported in trust-LiMe after the mettre_a_jour fix (Champ_Fonc_Fonction/Tabule vs. pb temperature) | ⚠️ (nice to have; fluid side via TRUST, eta via trust-LiMe) |
| Conducting wall BC (`paroi_conductrice_magnetique`) | ❌ Deferred | ⚠️ (impeller surfaces) |
| div-b enforcement | ⚠️ Opt-in projection for the reported cell-centred divergence; 2D scan passes, 3D/regression pending | ⚠️ (important at high Rm) |

## 5. Gap analysis: what must be built before a VKS data file can run

### 5.1 Critical gaps (must fix)
1. **Validated turbulent EDM closure:** effective η_t input and hydraulic
   k-ε handoff exist, but magnetic fluctuation transport and calibration for
   turbulent sodium are not implemented/validated.
2. **High-Rm validation**: the current low-Rm mode assumes b is a perturbation
   (b ~ Ha², not O(1)). VKS operates at Rm ~ 100-1000 where b is O(1).
   The induction equation must handle the nonlinear (u·∇)b and (b·∇)u terms
   at high Rm.
3. **High-Rm coupling validation:** the default staggered scheme can be
   supplemented by opt-in Picard iterations (`couplage_fort`), but stability
   and accuracy in the high-Rm regime remain untested; an implicit or
   semi-implicit strategy may still be needed.

### 5.2 Important gaps (should fix)
4. **Spherical mesh generation**: TRUST supports polyhedral meshes via
   `Polyedriser`. Need a mesh of the spherical shell with impeller surfaces.
5. **Impeller rotation BCs**: `Paroi_defilante` available and exercised in `SmoothDiskMHD` (smooth disks). Body force (`force_rotative`) not available in this TRUST version.
6. **div-b enforcement:** an opt-in projection cleans the module's reported
   divergence, but 3D validation remains and high-Rm robustness is unknown.
7. **Conducting wall BC**: impeller surfaces may be conducting (steel).

### 5.3 Nice-to-have
8. **Temperature-dependent properties**: for thermal effects.
9. **Magnetic probe integration**: for comparing with VKS measurements.
10. **Parallel scalability**: VKS meshes need 10⁶–10⁷ cells.

## 6. Proposed VKS data file skeleton

```text
# VKS simulation: two counter-rotating impellers, spherical shell
# Rm ~ 300, Re ~ 1e5, Ha ~ 10, Pm ~ 1e-5
dimension 3
Pb_MHD pb
Domaine dom

# --- Mesh: spherical shell with impeller surfaces ---
# BEGIN MESH #
Read_MED {
    domain dom
    file vks_shell.med
}
Polyedriser dom
# END MESH #

# --- Partition for parallel (hundreds of ranks) ---
# BEGIN PARTITION #
Partition dom
{
    Partition_tool metis { nb_parts $NPROCS }
    Larg_joint 2
    zones_name DOM
}
End
END PARTITION #
# BEGIN SCATTER #
Scatter DOM.Zones dom
END SCATTER #

# --- Time scheme ---
Schema_Euler_Implicite sch
Read sch
{
    tinit 0.
    tmax $TMAX
    dt_max $DT_MAX
    facsec 1e6
    facsec_max 1e6
    solveur implicite { solveur petsc cli { } }
}

Associate pb dom
Associate pb sch
Discretize pb dis

Read pb
{
    # --- Fluid (sodium) ---
    fluide_incompressible
    {
        mu      Champ_Uniforme 1 6.9e-4     # Pa.s
        rho     Champ_Uniforme 1 929        # kg/m3
        lambda  Champ_Uniforme 1 85         # W/m/K
        Cp      Champ_Uniforme 1 1380       # J/kg/K
        beta_th Champ_Uniforme 1 0.0
    }

    # --- Magnetic equation (trust-LiMe) ---
    Convection_Diffusion_Magnetic_field
    {
        diffusivite_magnetique Champ_Uniforme 1 0.078   # eta = 1/(mu0*sigma)
        vitesse_hydraulique                            # two-way coupling

        diffusion  { }
        convection { amont }

        sources
        {
            induction_stretching { }
            # turbulent_EDM { }   # deferred
        }

        boundary_conditions
        {
            wall_outer   paroi_isolante_magnetique
            wall_inner   paroi_isolante_magnetique
            impeller_top     paroi_isolante_magnetique   # or conductrice (B.2)
            impeller_bottom  paroi_isolante_magnetique
        }
        initial_conditions { magnetic_field Champ_Uniforme 3 0. 0. 1e-4 }
    }

    # --- Momentum equation (TRUST) ---
    Navier_Stokes_standard
    {
        solveur_pression Petsc Cholesky { }
        diffusion  { }
        convection { amont }

        # turbulence { k_epsilon { ... } }   # or les { ... }

        boundary_conditions
        {
            impeller_top     paroi_defilante Champ_Front_Fonc_xyz 3 0. $V_top 0.
            impeller_bottom  paroi_defilante Champ_Front_Fonc_xyz 3 0. 0. $V_bot
            wall_outer       paroi_fixe
            wall_inner       paroi_fixe
        }
        initial_conditions { vitesse Champ_Uniforme 3 0. 0. 0. }
        sources
        {
            force_lorentz { }
        }
    }

    # --- Post-processing ---
    Postraitement
    {
        Sondes
        {
            sonde_bx grav magnetic_field0 Periode 1e-3 Points 1 $X $Y $Z
            # ... (array of probes)
        }
        Champs
        {
            magnetic_field elem
            vitesse elem
            courant_magnetique elem
        }
        format lml
    }
}

Solve pb
```

## 7. Summary of inputs needed

| Category | Input | Source/Value |
|----------|-------|-------------|
| Geometry | Spherical shell mesh (.med) | Gmsh/Salome, r_i, r_o |
| Impellers | Surface tags in mesh | Blade geometry, count, pitch |
| Sodium | ρ, μ, σ, λ, C_p | ~929 kg/m³, 6.9e-4 Pa·s, 1.03e7 S/m, ... |
| Magnetic | η = 1/(μ₀σ), initial seed field | 0.078 m²/s, (0,0,1e-4) T |
| Impeller speed | V_top, V_bot (m/s) | Up to several m/s |
| Turbulence | Model choice (k-ε/LES), coefficients | k-ε: Cμ=0.09, etc. |
| Time | tmax, dt_max | 10-100 s, 1e-3-1e-2 s |
| Probes | Positions from VKS measurement array | (X,Y,Z) coordinates |
| BCs | Insulating/conducting walls, rotating impellers | paroi_isolante/conductrice, paroi_defilante |
| Parallel | NPROCS | Hundreds to thousands |

## 8. Prerequisites (what must be built first)

1. Turbulent EDM (η_t from k-ε) — critical
2. High-Rm validation of the induction equation
3. Tight (implicit) coupling for stability at high Rm
4. Spherical mesh generation pipeline
5. Impeller rotation BCs (`Paroi_defilante`) — available; exercised in `SmoothDiskMHD`
6. Conducting wall BC (paroi_conductrice, Phase B.2)
7. div-b enforcement (optional but recommended)
8. §18.2 kernel fix (for 3D form creation)
