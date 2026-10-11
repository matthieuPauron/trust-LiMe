# Problems, equations and boundary conditions

This page describes what the **TRUST-LiMe** module actually implements today, how it
maps onto the TRUST kernel, and what is deliberately *not* implemented yet.

---

## 1. The model solved in this first phase

TRUST-LiMe solves the **magnetic induction equation** in standalone form
(the fluid velocity is supplied as data, it is not solved here):

$$
\partial_t b + (u\cdot\nabla)b - \nabla\cdot(\eta\,\nabla b) = 0
$$

with

| Symbol | Meaning | Data keyword |
|---|---|---|
| $b$ | magnetic field (cell centred, `nb_composantes` components) | unknown |
| $u$ | transporting velocity (given, face centred) | `vitesse_magnetique` |
| $\eta$ | magnetic diffusivity $=1/(\mu_0\sigma)$ | `diffusivite_magnetique` |

This is the standard convection–diffusion form of the induction equation.
For an incompressible flow, the Gauss law $\nabla\cdot b=0$ is assumed by the
validation cases; **it is not enforced by the solver** (see §6).

### Physical model variants

| Model | Additional source term | Status |
|---|---|---|
| **M0** | none (pure advection–diffusion) | **implemented** |
| **M1** | $S=(b\cdot\nabla)u$ (frozen-velocity stretching) | **implemented** — kernel source term `induction_stretching` |
| M2+ | Lorentz force $(\nabla\times b)\times b$ on momentum | deferred — needs hydodynamics |
| M3 | vector-potential / divergence-free formulation | deferred |

---

## 2. Class map (maximum kernel reuse)

```
Pb_MHD  (Probleme_base)                         src/Pb_MHD.{h,cpp}
  └── eq_magnetic_field
        Convection_Diffusion_Magnetic_field     src/Convection_Diffusion_Magnetic_field.{h,cpp}
              └── Convection_Diffusion_Temperature_base
                    └── Convection_Diffusion_std
                          └── Equation_base
```

Only a handful of methods are overridden; everything else (diffusion and
convection operators, mass solver, time integration, boundary-condition and
source machinery, post-processing plumbing) is inherited from the kernel:

| Overridden method | Why |
|---|---|
| `discretiser()` | build the unknown (`magnetic_field`) and the transport velocity |
| `inconnue()` (×2) | return our own `Champ_Inc_base` |
| `diffusivite_pour_transport()` | return $\eta$ instead of the fluid conductivity $\lambda$ |
| `diffusivite_pour_pas_de_temps()` | return $\eta$ (time-step stability estimate) |
| `vitesse_pour_transport()` | return the data velocity instead of `probleme().get_champ("vitesse")` |
| `set_param()` | register `diffusivite_magnetique`, `vitesse_magnetique`, `nb_composantes` |
| `readOn()` | fill the velocity field, rename the mass coefficient |
| `domaine_application()` | report `"Thermique"` so kernel scalar CLs are accepted (§4) |

`get_time_factor()` is **not** overridden: the energy equation's $\rho C_p$
factor does not apply to a magnetic field, and the `Equation_base` default
(`1.`) is correct.

Because the assembler is never overridden, `Equation_base::assembler_blocs`
handles the `Polymac_p0` case directly (for a poly family, the concrete energy
equation delegates to it as well).

See `KERNEL_REUSE_NOTES.md` (repository root) for the kernel facts this design
relies on, with pointers to the exact kernel sources.

---

## 3. Data-file keywords

Inside the `Convection_Diffusion_Magnetic_field` block:

| Keyword | Type | Required | Default | Notes |
|---|---|:--:|---|---|
| `diffusivite_magnetique` | `Champ_Don_base` | **yes** | — | $\eta$. Must appear **before** `diffusion`. |
| `vitesse_magnetique` | `Champ_Don_base` | no | zero | Transport velocity $u$ (e.g. `Champ_Uniforme 2 0.2 0.`). |
| `nb_composantes` | int | no | space dimension | 1…dimension. Use `1` for a single magnetic component. |
| `diffusion` | block | inherited | — | Diffusion operator keyword. |
| `convection` | block | inherited | — | Convection operator keyword. |
| `sources` | block | inherited | — | Kernel source framework; enables the M1 `induction_stretching` term (§3bis). |
| `initial_conditions` | block | **yes** | — | `magnetic_field <Champ_...> ...` |
| `boundary_conditions` | block | **yes** | — | Kernel scalar CLs (§4). |

**Ordering matters:** keywords are processed in the order they appear in the
data file, so `diffusivite_magnetique` must be given before `diffusion` (the
diffusion reader immediately queries the diffusivity).

The unknown is named **`magnetic_field`**; its components are
`magnetic_field0`, `magnetic_field1`, … and they are directly usable as probe
names.

---

## 3bis. Source terms

The equation reuses the kernel `Source_base` framework, so any kernel source can
be attached; in addition the module provides the M1 inductive stretching term.

### `induction_stretching` — M1 model

Written in the induction equation's convection–diffusion form, an incompressible
flow produces the stretching source on the right-hand side:

$$
\partial_t b + (u\cdot\nabla)b - \nabla\cdot(\eta\nabla b)
= S,\qquad S = (b\cdot\nabla)u,\quad S_i=\sum_j b_j\,\partial_j u_i .
$$

```c
Convection_Diffusion_Magnetic_field
{
  diffusivite_magnetique Champ_Uniforme 1 0.
  vitesse_magnetique     Champ_Fonc_xyz dom 2 x 0
  diffusion  { }
  convection { amont }
  sources    { induction_stretching { } }
  ...
}
```

Implementation notes:

- `src/Terme_Source_Induction_stretching.{h,cpp}`, a `Source_base` subclass
  registered under the keyword `induction_stretching`.
- $u$ is the **frozen** transporting velocity already held by the equation, so
  $\nabla u$ is constant in time. It is built once in `initialiser()` with the
  kernel field **`gradient_vitesse`** (`Discret_Thyd::grad_u`) — the *same*
  $\nabla u$ the momentum equation would use, so the coupling stays consistent.
- Because $\nabla u$ is constant and the term depends on $b$, the source is
  treated **explicit (lagged)**: `dimensionner_blocs()` contributes nothing to
  the Jacobian and `ajouter_blocs()` only fills the right-hand side. The
  generic `Equation_base::derivee_en_temps_inco` already calls
  `les_sources.ajouter(secmem)`, so no equation-side override is required.
- The source lives on the cells (`elem` localisation), matching the cell-centred
  magnetic unknown.
- The gradient field layout is the kernel convention (2D: `dUdX,dUdY,dVdX,dVdY`),
  i.e. component $(i\cdot\mathrm{dim}+j)$ is $\partial u_i/\partial x_j$.
- The contraction $S_i=\sum_j b_j\,\partial_j u_i$ is factored into the
  header-only `src/Induction_stretching_kernel.h`, so this — the most
  error-prone index convention in the module — is covered by unit tests
  (`tests/UnitTests/unit_induction_kernel.cpp`) without a mesh or a solve.

Only the M1 term is implemented; the Lorentz force on momentum (M2) and the
divergence-free projection (M3) remain out of scope (§6).

---

## 4. Boundary conditions (reuse of kernel scalar CLs)

`Cond_lim_base::compatible_avec_eqn()` compares the equation's
`domaine_application()` with each boundary condition's list of supported
domains. The magnetic equation therefore reports **`"Thermique"`**, which makes
the existing kernel scalar CLs immediately usable — **no new CL class is
written**.

| Physical intent | Kernel keyword (reuse) | Data after the keyword |
|---|---|---|
| zero flux / insulated | `Neumann_Paroi_adiabatique` | *(nothing)* |
| zero flux (alias) | `Paroi` | *(nothing)* |
| imposed value at a wall | `Temperature_imposee_paroi` | `Champ_Uniforme <N> <v…>` |
| imposed value at an inflow | `Frontiere_ouverte_temperature_imposee` | `Champ_front_Fonc_xyz <N> <values>` |

Physics-specific magnetic BCs (e.g. insulating vs conducting walls in the
Hartmann problem) are **deferred**; when they are needed they will be thin
subclasses of the kernel CLs registered under magnetic keyword names.

---

## 5. Discretisation

- `Polymac_p0` — cell-centred (P0) elements, the single discretisation used.
- The unknown is created with `Discret_Thyd::concentration(..., nb_composantes,
  "magnetic_field")`, which sets the field nature to `multi_scalaire` and names
  the components. This is exactly the pattern used by the kernel's
  `Convection_Diffusion_Concentration`; **no new discretisation is introduced**.
- The transporting velocity is created with `Discret_Thyd::vitesse(...)`
  (face-centred), because `Convection_Diffusion_std::associer_vitesse()`
  `ref_cast`s the returned field to a `Champ_Inc_base`.

---

## 6. Known limitations / deferred work

1. **Gauss law is not enforced.** $\nabla\cdot b=0$ is assumed, not projected.
2. **No Lorentz force.** The momentum equation is untouched (there is no
   momentum equation in this phase), so $(\nabla\times b)\times b$ (M2) is absent.
3. **$u$ is frozen.** Even with the M1 `induction_stretching` source, the
   velocity is a given field: $b$ feeds back into $u$ only through that
   explicit, lagged term — never through the momentum equation.
4. **Boundary conditions reuse thermal CL keywords.** Functional but
   confusing; magnetic-named aliases are deferred.
5. **`Pb_MHD` has a single equation.** There is no coupled three-equation
   (momentum + energy + induction) problem yet.
6. **Velocity is given as data.** There is no feedback from $b$ to $u$.

---

## 7. Problems

`Pb_MHD` derives from `Probleme_base` (same minimal pattern as the kernel's
`Pb_Conduction`) and holds exactly **one** equation:

```
Pb_MHD pb
{
    fluide_incompressible { … }              # medium (a fluid is required)
    Convection_Diffusion_Magnetic_field { … }
}
```

The medium must still be a fluid because
`Convection_Diffusion_Temperature_base::associer_milieu_base()` requires a
`Fluide_base`, even though the equation itself uses no thermal property.