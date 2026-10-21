@page TrustLiMe_model Problems, equations and boundary conditions

This page describes what the **TRUST-LiMe** module actually implements today, how
it maps onto the TRUST kernel, and what is deliberately *not* implemented yet.

---

## 1. The model solved in this first phase

TRUST-LiMe solves the **magnetic induction equation** in standalone form (the
fluid velocity is supplied as data, it is not solved here):

\f[
\partial_t b + (u\cdot\nabla)b - \nabla\cdot(\eta\,\nabla b) = 0
\f]

with

| Symbol | Meaning | Data keyword |
|---|---|---|
| \f$b\f$ | magnetic field (cell centred, `nb_composantes` components) | unknown |
| \f$u\f$ | transporting velocity (face centred) | `vitesse_magnetique` (data field) or `vitesse_hydraulique` (solved NS velocity) |
| \f$\eta\f$ | magnetic diffusivity \f$= 1/(\mu_0\sigma)\f$ | `diffusivite_magnetique` |

This is the standard convection–diffusion form of the induction equation. For an
incompressible flow, the Gauss law \f$\nabla\cdot b = 0\f$ is assumed by the
validation cases; **it is not enforced by the solver** (see the *Known
limitations* section below).

### Physical model variants

| Model | Additional source term | Status |
|---|---|---|
| **M0** | none (pure advection–diffusion) | **implemented** |
| **M1** | \f$S = (b\cdot\nabla)u\f$ (frozen-velocity stretching) | **implemented** — kernel source term `induction_stretching` |
| **M2+** | Lorentz force \f$(\nabla\times b)\times b\f$ on momentum | **implemented** — source term `force_lorentz` (§3ter) |
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
| `diffusivite_pour_transport()` | return \f$\eta\f$ instead of the fluid conductivity \f$\lambda\f$ |
| `diffusivite_pour_pas_de_temps()` | return \f$\eta\f$ (time-step stability estimate) |
| `vitesse_pour_transport()` | return the transport velocity instead of `probleme().get_champ("vitesse")` — the `vitesse_magnetique` data field, or the solved NS velocity once `completer()` has rebound it |
| `set_param()` | register `diffusivite_magnetique`, `vitesse_magnetique`, `vitesse_hydraulique`, `nb_composantes` |
| `readOn()` | fill the velocity field, rename the mass coefficient |
| `domaine_application()` | report `"Thermique"` so kernel scalar CLs are accepted (see below) |

`get_time_factor()` is **not** overridden: the energy equation's \f$\rho C_p\f$
factor does not apply to a magnetic field, and the `Equation_base` default
(\f$1\f$) is correct.

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
| `diffusivite_magnetique` | `Champ_Don_base` | **yes** | — | \f$\eta\f$. Must appear **before** `diffusion`. |
| `vitesse_magnetique` | `Champ_Don_base` | no | zero | Transport velocity \f$u\f$ (e.g. `Champ_Uniforme 2 0.2 0.`). |
| `nb_composantes` | int | no | space dimension | 1…dimension. Use `1` for a single magnetic component. |
| `diffusion` | block | inherited | — | Diffusion operator keyword. |
| `convection` | block | inherited | — | Convection operator keyword. |
| `sources` | block | inherited | — | Kernel source framework; enables the M1 `induction_stretching` term. |
| `initial_conditions` | block | **yes** | — | `magnetic_field <Champ_...> ...` |
| `boundary_conditions` | block | **yes** | — | Kernel scalar CLs (see *Boundary conditions*). |

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

\f[
\partial_t b + (u\cdot\nabla)b - \nabla\cdot(\eta\nabla b)
= S,\qquad S = (b\cdot\nabla)u,\quad S_i = \sum_j b_j\,\partial_j u_i .
\f]

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
- \f$u\f$ is the **frozen** transporting velocity already held by the equation,
  so \f$\nabla u\f$ is constant in time. It is built once in `initialiser()`
  with the kernel field **`gradient_vitesse`** (`Discret_Thyd::grad_u`) — the
  *same* \f$\nabla u\f$ the momentum equation would use, so the coupling stays
  consistent.
- Because \f$\nabla u\f$ is constant and the term depends on \f$b\f$, the source
  is treated **explicit (lagged)**: `dimensionner_blocs()` contributes nothing
  to the Jacobian and `ajouter_blocs()` only fills the right-hand side. The
  generic `Equation_base::derivee_en_temps_inco` already calls
  `les_sources.ajouter(secmem)`, so no equation-side override is required.
- The source lives on the cells (`elem` localisation), matching the cell-centred
  magnetic unknown.
- The gradient field layout is the kernel convention (2D: `dUdX,dUdY,dVdX,dVdY`),
  i.e. component \f$(i\cdot\mathrm{dim}+j)\f$ is \f$\partial u_i/\partial x_j\f$.
- The contraction \f$S_i = \sum_j b_j\,\partial_j u_i\f$ is factored into the
  header-only `src/Induction_stretching_kernel.h`, so this — the most
  error-prone index convention in the module — is covered by unit tests
  (`tests/UnitTests/unit_induction_kernel.cpp`) without a mesh or a solve.

Only the M1 term is implemented; the divergence-free projection (M3) remains
out of scope.

---

## 3ter. Coupled low-Rm mode: the Lorentz force on momentum

`Pb_MHD` accepts **extra equations** after the magnetic one: any
`Equation_base` keyword written in the problem block (typically
`Navier_Stokes_standard`) is read by the kernel's optional-equation mechanism
(`Probleme_base::read_optional_equations`) and appended to the equation list,
exactly like the optional turbulence equations of `Pb_Hydraulique`.

```c
Pb_MHD pb
{
    nb_composantes 2
    fluide_incompressible { ... }
    Convection_Diffusion_Magnetic_field { ... }
    Navier_Stokes_standard                   # presence => coupled mode
    {
        solveur_pression Petsc Cholesky { }
        diffusion  { }
        convection { amont }
        boundary_conditions { ... }
        initial_conditions { vitesse Champ_Uniforme 2 0. 0. }
        sources { force_lorentz { } }
    }
}
```

`nombre_d_equations()` is \f$1\f$ standalone, \f$1 + \f$ (number of optional
equations) otherwise. The coupling itself is the **`force_lorentz` source**
(`src/Terme_Source_Lorentz.{h,cpp}`), a `Source_base` attached to the momentum
equation:

- \f$\mathbf{F} = \mathbf{J}\times\mathbf{b}\f$, \f$\mathbf{J} =
  \mu_0^{-1}\nabla\times\mathbf{b}\f$, computed from the **lagged** magnetic
  field (explicit low-Rm coupling, like `induction_stretching` — nothing in
  the Jacobian);
- \f$\nabla\mathbf{b}\f$ comes from the same Green-Gauss
  `gradient_b_elements()` used by the post-treatment fields (exact at interior
  cells, O(h) wall bias — see the divergence note above and
  `KERNEL_REUSE_NOTES.md`);
- divided by the fluid density: momentum sources are per **unit mass**
  (gravity/Boussinesq convention);
- assembled on the face-based velocity unknown following the kernel
  `Terme_Boussinesq_PolyMAC_CDO_Face` structure: face block (normal
  projection weighted by the upstream entrelace volumes and face porosity)
  plus, for `Polymac_p0`, the auxiliary element-velocity block
  (\f$\mathbf{F}\,\pi_e V_e\f$); Dirichlet-velocity faces are skipped in the
  face block;
- optional keyword `mu0 <value>` (default: the magnetic equation's \f$\mu_0\f$).

The source locates the magnetic equation by type inside the hosting problem
(`Terme_Boussinesq_base::associer_pb` pattern) and refuses to attach to a
problem without one.

**Validation:** `share/Validation/Rapports_automatiques/MagneticCoupled/
ForceLorentzBalance/` — with \f$\mathbf{b} = (y,0)\f$ (curl-free force), the
exact steady solution is \f$\mathbf{u}=0\f$, \f$p = -y^2/(2\mu_0^2\rho) + C\f$;
the notebook checks the pressure difference against the analytic
\f$-0.18\f$ (within 10 %: the discrete balance carries the O(h) Green-Gauss
wall bias) and the velocity relaxation \f$|\mathbf{u}| < 5\cdot10^{-4}\f$.

The feedback \f$\mathbf{u}\f$ \f$\to\f$ magnetic convection **is** wired: the
`vitesse_hydraulique` keyword on the magnetic equation makes its transport
velocity the *solved* NS velocity, rebound in `completer()` once every equation
is known (see §6, item 2). Without that keyword the mode stays one-way
\f$\mathbf{b}\f$ \f$\to\f$ momentum, transporting with the `vitesse_magnetique`
data field.

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

Two physics-specific magnetic BCs are now tracked:

- `paroi_isolante_magnetique` -- insulating wall (zero normal flux of `b`,
  all components). **Implemented** in Phase B.1 as a thin subclass of
  `Neumann_paroi_adiabatique`, registered under the magnetic keyword with
  app-domains `Magnetique` / `Thermique` / `indetermine`.
- `paroi_conductrice_magnetique` -- perfect conductor (`b x n = 0`,
  tangential components clamped to Dirichlet 0). **TBD (Phase B.2)**: the
  mixed per-component conditioning (Dirichlet on tangential vs. Neumann on
  the normal) is not expressible in the current one-CL-per-boundary CL model
  and needs a dedicated class at the PolyMAC face level. Deferred pending an
  API investigation; the insulating (`paroi_isolante_magnetique`) and
  zero-flux (`Neumann_Paroi_adiabatique` / `Paroi`) keywords are the
  available substitutes in the meantime.
| Physical magnetic wall | Best available today | Honest status |
|---|---|---|
| imposed field `b = b₀` | `Temperature_imposee_paroi` (Dirichlet, per component) | correct |
| zero‑flux / `∂b/∂n = 0` | `Neumann_Paroi_adiabatique` / `Paroi` | correct |
| insulating wall, zero normal flux | `paroi_isolante_magnetique` (homogeneous Neumann, every component) | **implemented** (Phase B.1) |
| strict `b·n = 0` (field tangent to the wall) | none (needs Dirichlet on the normal component only) | **deferred** |
| perfect conductor `b×n = 0` | none (needs a per-component CL class) | **deferred** (Phase B.2) |
| far‑field / vacuum outside | none (non‑local) | **deferred** |

---

## 5. Discretisation

- `Polymac_p0` — cell-centred (P0) elements, the single discretisation used.
- The unknown is created with
  `Discret_Thyd::concentration(..., nb_composantes, "magnetic_field")`, which
  sets the field nature to `multi_scalaire` and names the components. This is
  exactly the pattern used by the kernel's
  `Convection_Diffusion_Concentration`; **no new discretisation is introduced**.
- The transporting velocity is created with `Discret_Thyd::vitesse(...)`
  (face-centred), because `Convection_Diffusion_std::associer_vitesse()`
  `ref_cast`s the returned field to a `Champ_Inc_base`.

---

## 4bis. Post-treatment fields

The equation exposes the following **derived MHD fields**, computed on demand
from the solved magnetic field `b` and the transport velocity `u` (element P0
fields, same support as `magnetic_field`). They are registered through the
equation's `creer_champ` / `has_champ` / `get_champ` hooks, so they are usable
by name in `Postraitement` (`champs`, `Sondes`, `Statistiques`) like any
kernel field:

| Post field | Nature | Formula | Validity |
|---|---|---|---|
| `pulsation_cyclotronique` | scalar | \f$\omega_c = (q/m)\,|b|\f$ | any |
| `energie_magnetique` | scalar | \f$e_m = |b|^2 / (2\mu_0)\f$ | any |
| `vecteur_poynting` | vector | \f$S = \mu_0^{-1}\,(|b|^2 u - (u\cdot b)\,b)\f$ | needs the velocity |
| `helicite_croisee` | scalar | \f$h_c = u\cdot b\f$ | needs the velocity |
| `helicite_cinetique` | scalar | \f$h_k = u\cdot(\nabla\times u)\f$ | \f$\equiv 0\f$ in 2D (out-of-plane vorticity) |
| `nombre_de_reynolds_magnetique` | scalar | \f$Rm = |u|\,L_e/\eta\f$, \f$L_e = V_e^{1/d}\f$ | needs the velocity |
| `nombre_de_prandtl_magnetique` | scalar | \f$Pm = \nu/\eta\f$ | — |
| `rotationnel_magnetique` | 2D scalar / 3D vector | \f$\mathbf{w} = \nabla\times\mathbf{b}\f$ | needs \f$\nabla b\f$; **vector \f$b\f$ only** (scalar model rejected) |
| `divergence_magnetique` | scalar | \f$\nabla\cdot\mathbf{b} = \mathrm{tr}(\nabla b)\f$ | needs \f$\nabla b\f$; **vector \f$b\f$ only** |
| `courant_magnetique` | 2D scalar / 3D vector | \f$J = \nabla\times\mathbf{b}/\mu_0\f$ | needs \f$\nabla b\f$; **vector \f$b\f$ only** |
| `force_lorentz` | vector | \f$F = (\nabla\times\mathbf{b})\times\mathbf{b}/\mu_0\f$ | needs \f$\nabla b\f$; **vector \f$b\f$ only** |

Two equation keywords tune them (no influence on the transport itself):

| Keyword | Type | Default | Used by |
|---|---|---|---|
| `mu0` | float | \f$1\f$ | `energie_magnetique`, `vecteur_poynting` |
| `rapport_charge_masse` | float | \f$e/m_e = 1.75882001076\times 10^{11}\f$ C/kg (positron charge, electron mass) | `pulsation_cyclotronique` |

Conventions and deferred scope:

- The **magnetic Reynolds number** is the *local* number on the cell (the
  magnetic twin of the kernel `Reynolds_maille`, but at elements):
  \f$Rm_e = |u_e|\,L_e/\eta_e\f$ with \f$L_e = V_e^{1/d}\f$ the cell size. A
  *global* number is a user-supplied value (as for the hydraulic Re); supply it
  yourself with a `Definition_champs`/`Transformation` if needed.
- **Crossed helicity** and **Poynting** need the velocity: they are computed
  from the transport velocity — the `vitesse_magnetique` data field, or the
  solved NS velocity in two-way mode (zero if neither is specified).
- Fields in a *vector* magnetic equation (`nb_composantes 2` or more) include
  every transported component; the untransported components act as \f$0\f$
  (scalar model: \f$b\f$ has one component).
- The differential post-treatment fields (`rotationnel_magnetique`,
  `divergence_magnetique`, `courant_magnetique`, `force_lorentz`) all require
  the full vector magnetic field (\f$\nabla b\f$ via Green-Gauss reconstruction);
  the scalar model is rejected with a clean error by `creer_champ` — see
  `Champ_Fonc_MHD::calculer` and `Convection_Diffusion_Magnetic_field::creer_champ`.

Not implemented (need new discretisation machinery): `helicite_magnetique`
  and `helicite_generalisee` (need a vector-potential solve, M3).

Implementation: one field class, `src/Champ_Fonc_MHD.{h,cpp}` (a
`Champ_Fonc_Elem_PolyMAC_CDO` whose `mettre_a_jour()` dispatches on the field
name); the per-cell formulas are factored into the unit-testable,
framework-free `src/MHD_posttraitement_kernel.h`. Validation examples live in
`share/Validation/Rapports_automatiques/Postraitement_fields/`.

---

## 6. Known limitations / deferred work


1. **Gauss law is not enforced.** \f$\nabla\cdot b = 0\f$ is assumed, not
   projected.
2. **Two-way coupling implemented (`vitesse_hydraulique`).** The
   `force_lorentz` source (§3ter) applies \f$(\nabla\times b)\times b/\mu_0\f$
   to the momentum equation (explicit, lagged \f$b\f$), and with the
   `vitesse_hydraulique` keyword the magnetic equation's transport
   velocity is the solved NS velocity, rebound in `completer()` (the
   optional equation exists only once every equation is known). The
   scheme is fully lagged staggered: \f$b^{s+1}=F(b^s,u^s)\f$,
   \f$u^{s+1}=G(u^s,b^s)\f$. Validated by `Coupled_Feedback` (both modes
   in one form) and the `Error_TwoWay*` negative forms.
3. **\f$u\f$ in the magnetic equation.** With `vitesse_hydraulique` the
   velocity is the solved NS unknown; the `induction_stretching` source
   rebuilds grad(u) per step (`maj_grad_u()`), keyed on the velocity's
   own time (one rebuild per step, at assembly time — the source's
   `mettre_a_jour` runs *before* the NS wheel rotates).
4. **Boundary conditions reuse thermal CL keywords.** Functional but confusing;
   magnetic-named aliases are deferred.
5. **`Pb_MHD` holds the magnetic equation plus optional extras.** Standalone is
   a single equation; a Navier-Stokes equation may be appended for the coupled
   mode (§3ter). There is no three-equation (momentum + energy + induction)
   problem yet.
6. **Architecture note:** `Pb_MHD` is ill-named — it is the magnetic
   induction problem. The target is a `Pb_Hydraulique` + a renamed magnetic
   problem associated by a `Probleme_Couple` subclass; the velocity binding
   machinery above carries over unchanged (the NS locator
   `eq_navier_stokes()` is the single seam to revisit).

---

## 7. Problems

`Pb_MHD` derives from `Probleme_base` (same minimal pattern as the kernel's
`Pb_Conduction`) and holds the magnetic equation, optionally followed by extra
equations (§3ter):

```
Pb_MHD pb
{
    fluide_incompressible { … }              # medium (a fluid is required)
    Convection_Diffusion_Magnetic_field { … }
    # optional: Navier_Stokes_standard { … } for the coupled low-Rm mode
}
```

The medium must still be a fluid because
`Convection_Diffusion_Temperature_base::associer_milieu_base()` requires a
`Fluide_base`, even though the equation itself uses no thermal property.
