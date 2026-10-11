# trust-LiMe × trust-code kernel reuse notes

Running notes collected while building the standalone magnetic module. The goal is
**maximum reuse of trust-code, minimum new code**. Add to this file whenever a
kernel fact is rediscovered the hard way.

Paths below are relative to the trust-code checkout (`../trust-code` here).

---

## 0. Quick facts that repeatedly save time

| Fact | Detail |
|---|---|
| **Kernel include style** | `#include <Foo.h>` **with** the `.h` extension (aggregator `include/`). Our own headers use `"Foo.h"`. |
| **Real C++ standard** | The kernel is built with **`-std=c++20`** (see `compile_TRUST_*_opt_*.log`). The bundled `compile_commands.json` is **stale** (says `-std=c++17`) and makes Kokkos fail. `syntax-check.sh` now forces `-std=c++20`. |
| **Warnings are errors** | Module build uses `-Werror -Wall -Wextra -Wconversion -Wshadow -Woverloaded-virtual -Wnon-virtual-dtor -pedantic`. `-Wconversion` is the usual trap. |
| **Fast diagnostics** | `bash .opencode/scripts/syntax-check.sh src/Foo.cpp` (seconds). |
| **Field factories** | All live on `Discret_Thyd` (the discretisation base): `temperature(sch,z,ch,nb_comp)`, `concentration(sch,z,ch,nb_constituants,nom)`, `vitesse(sch,z,ch,nb_comp)`. They all funnel into `Discretisation_base::discretiser_champ(...)`. |
| **Kernel has no curl operator** | No rotation operator in `src/Kernel/Operateurs`. M0/M1 do not need it. |

---

## 1. Keyword reading is `Param`-based and ordered

`Equation_base::readOn` builds a `Param`, calls the virtual `set_param(param)`,
then `param.lire_avec_accolades_depuis(is)`:

```cpp
Entree& Equation_base::readOn(Entree& is) {
  Param param(que_suis_je());
  set_param(param);
  param.lire_avec_accolades_depuis(is);
  ...
}
```

Consequences:

- `set_param` is **`const`** and uses `const` pointers everywhere. Registering a
  mutable `Champ` member works because `OWN_PTR(T)` is `TRUST_Deriv<T>`, which
  derives (via `Objet_U_ptr`) from `Objet_U`, so `&member` binds to
  `Param::ajouter(const char*, const Objet_U*, ...)`.
  Example copied from `Milieu_base`: `param.ajouter("rho", &ch_rho_);`.
- **Keywords are processed in file order.** A keyword whose handler *queries*
  another member must appear **before** it. In the magnetic block,
  `diffusivite_magnetique` must precede `diffusion` (the diffusion reader calls
  `diffusivite_pour_transport()`).
- Non-standard keywords (`diffusion`, `convection`, `sources`,
  `conditions_limites|boundary_conditions`, `conditions_initiales|initial_conditions`)
  are declared with `param.ajouter_non_std(name, this)` and dispatched to
  `lire_motcle_non_standard`.
- `Propagate`: `Equation_base::set_param` marks `boundary_conditions` and
  `initial_conditions` as `REQUIRED`.

---

## 2. Problem/equation lifecycle (order matters)

`Probleme_base::readOn` (see `Probleme_base.cpp`):

1. `lire_solved_equations(is)`
2. `typer_lire_milieu(is)`:
   - reads **one** medium (`fluide_incompressible { … }`),
   - `associer_milieu_base(medium)` → for an equation, sets `le_fluide`,
   - `discretiser_equations()` → `equation(i).discretiser()`,
   - `equation(i).associer_milieu_equation()`,
   - `equation(0).milieu().discretiser(pb, discretisation)`.
3. `lire_equations(is, motlu)` — reads exactly `nombre_d_equations()` blocks,
   matching the block keyword against `equation.que_suis_je()`.
4. `completer()` then `verifier()`.

So **`discretiser()` runs before the equation block is read**, but **after**
the fluid is associated. `discretiser()` must therefore create the unknown and
any operator-support fields; `readOn` fills data read from the block.

`Equation_base::discretiser()` also creates the time-derivative field and types
`solveur_masse`, `le_dom_Cl_dis`; call it **after** creating the unknown.

---

## 3. Boundary-condition compatibility

`Cond_lim_base::compatible_avec_eqn` compares the equation's
`domaine_application()` against each CL's `app_domains` vector. To reuse an
existing CL family, the equation must report a domain that family declares.

- Thermal scalar CLs (`paroi_adiabatique`, `frontiere_ouverte_temperature_imposee`,
  `Neumann_paroi`, `Scalaire_impose_paroi`, …) declare `"Thermique"` (many also
  declare `"indetermine"`).
- The magnetic equation therefore reports **`domaine_application() == "Thermique"`**
  so it can reuse kernel scalar CLs with **zero new CL code**.
- `Paroi_adiabatique` is a thin user CL whose `complement()` types the actual
  `Neumann_Paroi_adiabatique`; `paroi_flux_impose` → `Neumann_Paroi`.

Dedicated magnetic CL keyword aliases are deferred (they would need small new
classes). See `PLAN_magnetic_standalone.md` (D4).

---

## 4. Transport velocity must be a `Champ_Inc_base`

`Convection_Diffusion_std::associer_vitesse(const Champ_base&)` does:

```cpp
la_vitesse_transportante = ref_cast(Champ_Inc_base, vit);   // runtime checked!
```

despite taking a `Champ_base&`. So `vitesse_pour_transport()` **must** return a
`Champ_Inc_base`. A plain `Champ_Don_base` (e.g. `Champ_Uniforme`) is **not**
accepted. Recipe used here:

- create the velocity with `Discret_Thyd::vitesse(sch, dom, ch, 1)` (face-centred
  vector field) in `discretiser()`, and
- fill it from the data with `champ_inc->affecter(champ_don)` (same call the
  initial-condition reader uses: `Equation_base.cpp: inconnue().affecter(ch_init.valeur())`).

The operator `Operateur_Conv::associer_vitesse(const Champ_base&)` itself stores
`OBS_PTR(Champ_base)`, so it is not the bottleneck — only the equation's
`associer_vitesse` is.

---

## 5. Multi-component scalar fields

To transport several independent scalars (here the components of **b**), reuse
`Discret_Thyd::concentration(sch, dom, ch, nb_comp, "name")`: it sets the field
nature to `multi_scalaire` and names the components `name0,name1,…`. This is the
exact pattern of `Convection_Diffusion_Concentration`. No new discretisation.

## 6. Assemblage / time integration — inherit, do not rewrite

For `Polymac_p0` (`discretisation().is_poly_family() == true`):

- `Equation_base::assembler_blocs` is generic (loops over the 2 operators +
  sources). `Convection_Diffusion_Temperature::assembler_blocs` just delegates
  to it for the poly family.
- `Equation_base::derivee_en_temps_inco` is generic; `Convection_Diffusion_Temperature::derivee_en_temps_inco`
  delegates to it when `is_penalized == 0`.
- The only energy-specific extra is the `rho*Cp` time factor. We neutralise it by
  overriding `get_time_factor()` → `1.` (the `Equation_base` default is already
  `1.`).

Hence the magnetic equation implements **no** assembler/derivative code.

---

## 7. Equation base-class choice

`Convection_Diffusion_Temperature_base` (not the concrete
`Convection_Diffusion_Temperature`) is the sweet spot:

- gives `le_fluide`, `associer_fluide`, `fluide()`, `milieu()`,
  `associer_milieu_base` (accepts any `Fluide_base`);
- gives `diffusivite_pour_transport()` / `diffusivite_pour_pas_de_temps()`
  hooks (default to the fluid's λ/α) to override with η;
- inherits generic `Equation_base` assembler/derivative/BC/source machinery;
- avoids the concrete class's thermal checks (λ, Cp, β), `rhoCp`, penalisation
  and `gradient_temperature` machinery.

Problem shell: derive from `Probleme_base` like `Pb_Conduction`
(`src/ThSol/Pb_Conduction.{h,cpp}`) — implement only
`nombre_d_equations()`, `equation(int)` (const + non-const), and the
instanciation macros. `Probleme_base::associer_milieu_base` already loops over
the equations.

---

## 8. Handy reference files

| Need | File |
|---|---|
| Equation block reading | `src/Kernel/Framework/Equation_base.{h,cpp}` |
| Problem lifecycle | `src/Kernel/Framework/Probleme_base.cpp` |
| Minimal single-equation problem | `src/ThSol/Pb_Conduction.{h,cpp}` |
| Multi-scalar transport pattern | `src/ThHyd/Incompressible/Equations/Convection_Diffusion_Concentration.{h,cpp}` |
| Thermal equation (extra machinery to avoid) | `src/ThHyd/Incompressible/Equations/Convection_Diffusion_Temperature.cpp` |
| CL compatibility | `src/Kernel/Framework/Cond_lim_base.cpp` |
| Param / keyword registration | `src/Kernel/Utilitaires/Param.h`, `Milieu_base.cpp` |
| Field factories | `src/ThHyd/Incompressible/Discret_Thyd.{h,cpp}`, `src/ThSol/Discret_Thermique.cpp` |
| Source framework | `src/Kernel/Framework/Source_base.{h,cpp}`, `Sources.cpp` |
| Reference source impls | `src/ThHyd/Incompressible/Sources/Terme_Boussinesq_base.{h,cpp}` |

---

## 7bis. Writing a `Source_base` term (hard-won details)

Contract for a source attached to an equation's `sources { ... }` block:

1. **`readOn` must consume its own braces.** `Sources::readOn` does
   `is >> so.valeur()` then expects `}` or `,`. Returning immediately from
   `readOn` (without reading `{ }`) breaks the parse — use
   `Param param(que_suis_je()); set_param(param); param.lire_avec_accolades_depuis(is);`
   even when there are no keywords.
2. **`associer_domaines()` / `associer_pb()` are called for you.**
   `Source_base::completer()` (via `Sources::completer()` ←
   `Equation_base::completer()`) calls both with the equation's domain and
   problem. `Equation_base::discretiser()` does **not**.
3. **Explicit sources need only `ajouter(secmem)`.**
   `Equation_base::derivee_en_temps_inco` already calls
   `les_sources.ajouter(secmem)`; no equation-side override is needed.
   (`derivee_en_temps_inco_sources` is a *different*, empty virtual — only IBM
   equations use it. Do not be misled by it.)
4. **PolyMAC uses the bloc matrix path**, so `Source_base::ajouter_blocs` /
   `dimensionner_blocs` **must** be overridden — their base implementations
   `Process::exit("... not coded!")`. For an explicit term, `dimensionner_blocs`
   is empty and `ajouter_blocs` just fills the RHS.
5. `Declare_instanciable` already declares `printOn`/`readOn`; do **not**
   redeclare them (duplicate-declaration error). It does **not** declare
   `localisation_source` (that lives on `Source_Generique_base`).
6. `has_interface_blocs()` returns **`int`**, not `bool`.

### Getting a velocity gradient for free

`Discret_Thyd::grad_u(z, zcl, ch_vitesse, ch)` builds the kernel field
**`gradient_vitesse`** (`grad_U_Champ_Face_PolyMAC_CDO`, cell-centred, with
`dim*dim` components named `dUdX,dUdY[,dVdX,dVdY[,dWdZ]]`, component
`i*dim+j` = `du_i/dx_j`). This is the *same* ∇u the momentum equation uses, so
source terms and momentum stay consistent. The base `Discret_Thyd::grad_u`
exits cleanly if a discretisation does not implement it.

---

## 7ter. Unit tests (baltik modules)

The TRUST kernel tests its internals with **GoogleTest** in
`trust-code/tests/UnitTests/` (`unit_*.cpp`, `TEST(Suite, Name)`,
`EXPECT_*` / `EXPECT_DEATH`), built by `make unit` at the TRUST root.

Things worth knowing if you add tests to a **baltik module**:

- **baltik generates no `unit` target.** `tests/UnitTests/CMakeLists.txt` of the
  kernel relies on `${libtrio}`, `${libdeps}`, `${syslib}`, which only exist
  inside the baltik configure environment — you cannot reuse it standalone.
- **The two reusable artefacts are already on disk:**
  - compile flags → `compile_commands.json`; `syntax-check.sh` exposes them via
    `SYNTAX_CHECK_PRINT_FLAGS=1` (and `EXTRA_CXXFLAGS=...` to add paths, which
    is how the gtest include dir is added);
  - link line → `build/src/exec_opt/CMakeFiles/exe.dir/link.txt`.
    `.opencode/scripts/unit-tests.sh` rewrites the object list in place and
    reuses everything else.
- **Objects must be substituted in place**, not appended: with static archives,
  objects listed after the libraries resolve nothing.
- **Static link traps hit in this repo:** `mpicxx` shells out to
  `$TRUST_ROOT/exec/ccache/g++`, so `TRUST_ROOT` must be exported or the link
  dies with `/exec/ccache/g++: No such file or directory`.
- `readOn()` is **protected**; drive it through `Entree::operator>>(Objet_U&)`,
  i.e. `e >> obj`, exactly as the framework does.
- `le_type()` returns the **registered data-file keyword**; assert on it to pin
  the vocabulary.
- A pattern that works well here: factor a hard-to-reach piece of logic into a
  header-only function (`src/Induction_stretching_kernel.h`) so it can be tested
  without a mesh, a solver or even the standard kernel.

---

## 9. Open items / gotchas found

- `compile_commands.json` is stale (`c++17`); always force `-std=c++20`.
- `TRUST_Deriv` (OWN_PTR) uses `operator bool` / `.valeur()` / `.non_nul()`?
  → **`non_nul()` does not exist on OWN_PTR**; use `if (ptr)` and `ptr.valeur()`.
  (`OBS_PTR` has `non_nul()`.)
- `Paroi_adiabatique` app-domain compatibility is resolved through its
  `complement()`-typed `Neumann_*` class; keep `domaine_application()` aligned
  with the thermal domain for reuse.

---

## 10. Writing self-contained TRUST test cases (no `.med` needed)

Useful when a validation case must run from a clean checkout:

```c
dimension 2
Pb_MHD pb
Domaine dom

Mailler dom {
    Pave pave {
        Origine          0. 0.
        Nombre_de_Noeuds 17 65
        Longueurs        1. 1.
    }
    {
        Bord gauche  X = 0.  0. <= Y <= 1.
        Bord bas     Y = 0.  0. <= X <= 1.
    }
}
Decouper dom { Partitionneur tranche { tranches 2 4 } Larg_joint 2 Nom_Zones dom }
Fin
Scatter dom.Zones dom
```

- `Nombre_de_Noeuds` is the **node** count, so cells = nodes − 1.
- Reference: `Outils/ICoCo/.../CouplageFluideSolide/src/CouplageTrust/docond_VEF.data`.

### Field syntax

- `Champ_Uniforme <N> <v1 … vN>`
- `Champ_Fonc_xyz dom <N> <expr…>` — each `<expr>` is a **Parser_U** expression in
  `x`, `y` (and `z` in 3D), e.g. `sin(pi*y)`, `exp(-((x-0.25)^2)/(2*0.05^2))`.
  Evaluated at element centres → matches a P0 unknown.

### Point probes (`Postraitement/Sondes`) — the reliable way to validate

```c
Postraitement {
    Sondes {
        sonde_y050 grav magnetic_field Periode dts Points 1 0.5 0.50
    }
}
```

- syntax: `<nom_sonde> [special] <nom_champ> Periode <per|keyword> Points <n> x y [z] …`
- `special` ∈ `grav` (nearest cell centre) / `som` (nearest vertex) / `nodes` (nearest face).
- output file: **`<nom_sonde>.son`**, readable in Python with
  `trustutils.files.SonFile(path, None).getValues(entry)` → `(time, values)`.
- results land in `run.BUILD_DIRECTORY/<dataset name>/`.
- For a multi-component unknown, a component can be selected by name
  (`magnetic_field0`).

Because probes are snapped to mesh points, prefer **ratios** (`b(t)/b(0)`) over
absolute values when comparing against an analytic solution: the reference then
does not depend on the exact probe location.

## 11. Validation workflow

```bash
# module must be built first
make module_semi_optim
# then, inside the validation folder, in the TRUST python env:
source <repo>/env_trust_LiMe.sh
source ../../../trust-code/env_for_python.sh
source ~/Development/venv_trust/bin/activate
jupyter nbconvert --to notebook --execute --output-dir /tmp/nb <notebook>.ipynb
```

The `MagneticStandalone` notebook uses `assert`, so an `nbconvert --execute`
failure is a genuine test failure (not just a plot mismatch).

### Business-logic contracts (what the tests actually pin)

"Testing" here means the **modelling decisions**, not the discretisation
dimensions. Scope is 2D. For each decision: is it pinned by a test, and where?

| # | Modelling decision | Pinned? | By |
|---|---|:--:|---|
| 1 | **No $\rho C_p$ factor.** The equation is magnetic, not thermal; the time factor must be exactly 1. This is the single most dangerous inherited decision (the base class is the *energy* equation). | ✅ | `diffusion`, `diffusion_refined` — the *absolute* decay rate $e^{-\eta\pi^2t}$ pins the constant; a $\rho C_p$ factor would be off by ~10³ |
| 2 | **η is used verbatim** as the diffusion coefficient (not a transformed quantity). | ✅ | `diffusion` (rate), `steady_state` (divergence form) |
| 3 | **η is used for the operator *and* for the time-step estimate** (`diffusivite_pour_transport` / `..._pour_pas_de_temps` both return η). | ⚠️ partial | only indirectly (a wrong value changes dt, not the converged solution) |
| 4 | **Components are transported independently** — no coupling, no projection. | ✅ | `vector_components` (two distinct eigen-rates) |
| 5 | **No spurious source**: equilibria are preserved exactly. | ✅ | `invariance` (1e-10), `steady_state` (1e-8) |
| 6 | **Diffusion is in divergence form** $\nabla\!\cdot\!(\eta\nabla b)$, so a spatially varying η behaves correctly. | ✅ | `steady_state` ($\eta=1+x$, exact equilibrium) |
| 7 | **Velocity is given data and is frozen**; no feedback from $b$ to $u$. | ✅ | `advection`, `varying_velocity` |
| 7b | **M1 source $S=(b\cdot\nabla)u$** has the right magnitude, sign, gradient-index order and per-component structure. | ✅ | `source_stretching` ($b_0=B_0e^t$, $b_1\equiv0$) |
| 7c | The source is **explicit (lagged)** and adds nothing to the Jacobian. | ✅ by construction | `dimensionner_blocs()` is empty; verified by the growth rate matching $e^t$ to <1% |
| 8 | **BC semantics**: Dirichlet prescribes $b$, Neumann means zero *flux* $\partial b/\partial n=0$. | ✅ | implied by `diffusion` (a wrong Dirichlet would change the eigenvalue set, so the analytic rate would not match) |
| 9 | **Only insulated (Neumann) and prescribed (Dirichlet) walls are supported** — the conducting-wall condition (interface current / Ohm's law) is out of scope. | 📋 declared | `docs/content/model.md` §6 |
| 10 | **$\nabla\cdot b=0$ is assumed, not enforced** (Gauss law is the user's responsibility). | ❌ **not testable** | see the caveat below |
| 11 | Malformed input fails cleanly with a readable message. | ✅ | `error_missing_eta`, `error_bad_nbcomposantes`, `error_bad_bc` |
| 12 | Equation form is advection–diffusion, **not** the $-\nabla\times(u\times b)$ form. | 📋 by review | `docs/content/model.md` §1 |

### Why decision 10 cannot be tested in 2D (and what to do about it)

The Gauss law cannot be verified by a test case, and this is a genuine
limitation of the current design rather than an oversight:

- A divergence-free field that vanishes on the **whole** boundary must be
  identically zero in 2D: the div–curl identity gives
  $\int|\nabla b|^2 = \oint b\cdot n\,dl = 0$.
- So a non-trivial divergence-free transient needs mixed BCs. With a single
  component varying only in $y$ (so $\nabla\cdot b=\partial_y b_x=0$), the other
  component must be $y$-independent — which demands **different BCs per
  component**, while TRUST scalar CLs apply one uniform condition to all
  components.

Consequences for how the cases must be read:

| Case | Physically admissible magnetic state? |
|---|---|
| `invariance`, `diffusion`, `steady_state`, `varying_velocity` | **yes** ($\nabla\cdot b=0$) |
| `vector_components` | **no** — machinery test for multi-component handling |
| `advection` | **no** — convection-operator test only |

If Gauss-law correctness ever becomes a requirement, it needs an M3-style
projection or a vector-potential formulation, plus **per-component boundary
conditions**. Both are deferred (see `docs/content/model.md` §6).

Useful technique: give two components or two runs **different** analytic
behaviour (different eigen-rates, coarse vs refined mesh) so a plumbing bug
cannot hide behind symmetry.
