# trust-LiMe × trust-code kernel reuse notes

Running notes collected while building the standalone magnetic module. The goal is
**maximum reuse of trust-code, minimum new code**. Add to this file whenever a
kernel fact is rediscovered the hard way.

Paths below are relative to the trust-code checkout (`../trust-code` here).

> **This checkout is a *fork* of trust-code, not the official tree.** Maximum reuse
> and minimum new code remain the default, but the kernel **may** be modified when
> reuse is genuinely impractical. Every kernel change must pass a test gate — see
> §0bis.

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

## 0bis. Forked trust-code: when and how to modify the kernel

The `trust-code` checkout is a **fork**, not the official CEA tree:

| Remote | URL | Role |
|---|---|---|
| `origin` | `git@gitlab.com:matthieuPauron/trust-code-for-trust-li-me.git` | the fork — all work is pushed here |
| `upstream` | `git@gitlab.com:cea-trust-platform/trust/trust-code.git` | official CEA repo — **read-only** |

`upstream` has **no push URL** (`remote.upstream.pushurl` is empty), so any
`git push upstream …` fails by construction; `git fetch upstream` still works for
staying in sync. This is what keeps an accident from ever reaching the official
project.

### Policy

Maximum reuse is still the default: prefer subclassing/overriding in
`trust-LiMe/src` over touching the kernel. But because this is a fork, a kernel
change is **allowed** when reuse is impractical, under three conditions:

1. **Minimal** — change only what the module needs; no reformatting, no drive-by
   refactors, no touching numerical kernels unless *that* is the point.
2. **Non-regressing** — the kernel's own test suite must stay green (test gate below).
3. **Documented** — record the change and its rationale in this file, and consider
   proposing it upstream.

### Test gate (mandatory before *and* after any kernel edit)

Kernel source edits are only valid if the kernel tests do not regress:

```bash
cd ../trust-code
make check_optim      # baseline — must be green BEFORE you start
# … edit kernel files on a dedicated branch (see workflow) …
make check_optim      # must still be green AFTER
```

Then prove the module still builds and validates against the modified kernel:

```bash
cd ../trust-LiMe
make module_semi_optim
# re-run the unit tests and validation notebooks (trust-lime-testing skill)
```

A kernel change that breaks a kernel test case is **not acceptable** — fix it or
drop it. See the `trust-lime-testing` skill for the green-baseline discipline.

### Workflow

1. Branch in the fork for each kernel change: `git checkout -b kernel/<topic>`
   (never edit `master`/`next` directly).
2. Capture the green baseline first (`make check_optim`).
3. Make the **minimal** edit; prefer an added header/source or a small override
   over an invasive rewrite.
4. Re-run the kernel suite (gate above), then rebuild and validate the module.
5. Append the finding to this file; if the change is upstream-worthy, open a Merge
   Request against the official repo — the local `upstream` stays read-only and the
   MR goes through maintainer review.

### Trade-off to remember

Reusing the kernel means inheriting its test coverage for free. The moment you
modify it, you own that maintenance burden: every future `git fetch upstream` may
conflict, and you can no longer assume the kernel behaves exactly as upstream.
Keep every change small, justified, and easy to rebase.

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

## 9. Writing a `Source_base` term (hard-won details)

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
7. **Every source holding a field must override `mettre_a_jour()` /
   `resetTime()`** to forward them to the field. `Sources::mettre_a_jour` /
   `resetTime` call each source unconditionally, and the `Source_base`
   implementations `exit()` with "You must to overload ...". An empty forwarding
   body suffices when the field is time-independent.
8. **`secmem` is the *integrated* RHS** in both the legacy `ajouter` and the
   bloc `ajouter_blocs` path. A volumetric source must therefore be weighted by
   the cell measure `porosite_elem() * volumes()` before `secmem(e,i) += ...`
   (convention of `Source_Generique_P0_Elem::ajouter` /
   `Terme_Source_Decroissance_Radioactive_Elem_PolyMAC_CDO`). Omitting the
   weight divides the source by the cell volume in the update and the explicit
   integration blows up (residual ~1e35).
9. **Use a divergence-free transporting velocity for source-only tests.**
   TRUST's `convection { amont }` is *conservative* (`∇·(ub) = (u·∇)b +
   b∇·u`), so for a uniform field and a non-solenoidal u (e.g. `u=(x,0)`,
   `∇·u=1`) it contributes a spurious `−b∇·u` decay that masks the source.
   With `u=(x,−y)` (`∇·u=0`), convection vanishes for a uniform b and the
   source is measured cleanly.

### Getting a velocity gradient for free

`Discret_Thyd::grad_u(z, zcl, ch_vitesse, ch)` builds the kernel field
**`gradient_vitesse`**, the *same* ∇u the momentum equation uses, so source
terms and momentum stay consistent. The base `Discret_Thyd::grad_u` exits
cleanly if a discretisation does not implement it.

**Layout of the resulting field (hard-won — do not assume `(ne, dim²)`):**
the concrete field is `grad_Champ_Face_PolyMAC_MPFA`, a *face* field. Its
`valeurs()` has `nb_faces_tot() + nb_elem()*dim` rows and `N*dim` columns:
- face rows `[0, nb_faces_tot)`: gradient at faces,
- row `nb_faces_tot + e*dim + j`, column `i` = **d u_i / d x_j** for element e
  and phase n=0 (i.e. `du_i/dx_j  =  gu(nf_tot + e*dim + j, i)`, **note the
  transpose** relative to the natural `i*dim+j` per-element layout).

Consequently, a consumer wanting a per-element `(ne, dim*dim)` `[du_i/dx_j]`
table must re-extract with `g(e, i*dim+j) = gu(nf_tot + e*dim + j, i)`, where
`nf_tot = ref_cast(Domaine_VF, eqn.domaine_dis()).nb_faces_tot()` (see
`Terme_Source_Induction_stretching::ajouter_source` for the full pattern).

**Hard prerequisite: the velocity's *auxiliary element variables* must exist, or
`grad_u` silently reads out-of-bounds garbage.** `grad_Champ_Face_PolyMAC_MPFA::
calc_gradfve()` reads the velocity's **element** values from `vel.passe()` at
extended rows `nf_tot + e*dim + d` — those rows only exist if the field has been
extended to the `mdv_ch_face` MD_Vector (`md_vector_faces()` ⊕
`md_vector_elements()×dim`, i.e. `nb_faces_tot + nb_elem×dim` rows). On the
`Champ_Face_PolyMAC_MPFA` this extension is done by `init_auxiliary_variables()`
(public), which is normally triggered by a **Face** operator's `completer()`
(e.g. `Op_Conv_EF_Stab_PolyMAC_MPFA_Face::completer`). For the standalone
magnetic equation the unknown is an Elem field, so its convection/diffusion
operators are Elem-based (`Op_Conv_Amont_PolyMAC_HFV_Elem`,
`Op_Diff_PolyMAC_MPFA_Elem`) and *nobody* requests the velocity's aux variables:
`vel.valeurs()` keeps `nb_faces_tot` rows and `grad_u` reads past the end —
yielding garbage values like `8`/`0` instead of `∇u`.
Fix (used in `Terme_Source_Induction_stretching::initialiser`): guard with
`sub_type(Champ_Face_PolyMAC_MPFA, vit)`, call `vit.init_auxiliary_variables()`,
then re-drive `vit.update_ve(vit.futur(n))` for every temporal case — because
`update_ve()` gates on the MD_Vector of the *current* case only, while
`calc_gradfve` reads the `passe()` case. With this in place
`SourceStretching` gives `b(0.5)=1.6468` vs `exp(0.5)=1.6487` (~0.1%, the
implicit-Euler time error).

**But the element rows are *not* a flux-consistent gradient, and they are
biased at wall-adjacent cells.** `update_ge()` builds the element rows by
*averaging the face gradients*,
`val(nf_tot + D*e + d, n) = Σ_f (e==f_e(f,0)?1:-1) * fs(f)/ve(e) * (xv(f,d)-xp(e,d)) * val(f,n)`,
and in `calc_gradfve()` a **boundary face only receives a Dirichlet
contribution** (`fcl(f_bord,0) == 3`). A *frozen* velocity field carries no
hydrodynamic wall condition — the module associates it with the magnetic
equation, whose walls are Neumann — so every wall face of the velocity is built
from the element term alone, with no wall datum; that is consistent with the
measured wall-cell bias, which then lands in the element (wall-cell) gradient
through `update_ge()`.

Measured by **isolating the source**: run with `eta = 0`, so
`∂_t b_x = S_x = B0 ∂_y u_x` and the probe value at `t=1` *is* the discrete
`∂u_x/∂y` the source sees (`b_x(1) = B0 * ∂_y u_x`).

- with a **linear** velocity `u = 0.3 y` (exactly representable on faces, so any
  consistent scheme is exact): the implied `∂u_x/∂y` is `0.14997` in the **two
  wall-adjacent cells** and `0.29994…0.300` in the interior — a clean **factor
  2**, independent of the mesh;
- with the Hartmann profile (`Ha = 5`, 64 cells): implied `0.865` vs exact
  `1.875` in the wall cell (`0.974` vs `1.988` at 256 cells) — i.e. ≈ half, and
  mesh-independent, while the next cells in are already `O(1%)` or better.

Consequence for a wall-driven case (see `MagneticStandalone/Hartmann1D`): the
induced field departs from its analytic profile by an almost exactly **linear
mode in `y`** (`b_x^sim − b_x^analytic ≈ 0.305 y`, i.e. it vanishes at the duct
centre and is largest at the walls). It **converges, first order** (halves per
mesh refinement: `9.3e-2 → 4.8e-2 → 2.4e-2` of the profile amplitude at the wall
cells for 64/128/256 cells), and it is **odd**, so the mean and the odd symmetry
of the profile survive exactly. Practical rules:

- when comparing an induced field against an analytic profile, **keep the probes
  out of the two cells adjacent to a wall** (this mirrors the existing rule for
  post-treatment fields, §11) — the core then matches at the scheme's design
  order;
- the `Hartmann1D` forms compare tightly in the core and only **bound** the
  layer deviation, as a regression guard.

Candidate fixes (not attempted — would need a kernel/test-gate decision):
give the frozen velocity a **Dirichlet CL at the walls** so `calc_gradfve()`
sees it (the no-slip datum exists), or reconstruct `∇u` in the module itself by
Green–Gauss from the face values (as `gradient_b_elements` already does for
`b`), which is exact for a linear velocity and never touches the kernel.

---

## 10. Unit tests (baltik modules)

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

## 11. Registering a post-treatment field (Champ_Fonc) on an equation

The recipe to expose a derived field (e.g. `energie_magnetique`) so that
`Postraitement { champs { f elem } }`, `Sondes` and `Statistiques` can use it
by name (mirrors `Navier_Stokes_std::{creer_champ,has_champ,get_champ}`):

1. **Create lazily in the equation's `creer_champ(const Motcle&)`** (called at
   Postraitement/*Sondes* read time through `refChamp { Pb_champ pb <name> }`,
   which does `pb.creer_champ(nom)` then `pb.get_champ(nom)`). A P0 element
   field on the PolyMAC family is a `Champ_Fonc_Elem_PolyMAC_CDO` — either use
   that class directly or subclass it (we use `src/Champ_Fonc_MHD.h`, with a
   `mettre_a_jour(tps)` override which recomputes the values). In `creer_champ`:
   `champ.typer("Champ_Fonc_MHD")`, configure it (name, nature, nb_comp,
   `associer_domaine_dis_base(z)` **before** `fixer_nb_valeurs_nodales`), then
   `champs_compris_.ajoute_champ(champ)`.
2. **`Champs_compris_::ajoute_champ` indexes the field by `le_nom()` (both
   casings) *and* by every component name** (e.g. `vecteur_poynting` and
   `vecteur_poynting_x`), so component probes are handled by the kernel.
3. **Override `has_champ(nom, ref_champ)` to call your `get_champ` first.**
   `Probleme_base::get_champ` resolves fields through
   `equation(i).has_champ(nom, ref)` — and the default
   `Equation_base::has_champ` merely hands over the registered field **without
   recomputing it**. If you only recompute inside `get_champ`, the value
   written at t=0 is whatever the field held at creation (we observed a zero
   first probe line until the field refreshed). Our `has_champ(nom, ref)`
   therefore does `ref_champ = get_champ(nom)`.
4. **Advertise the names in `get_noms_champs_postraitables(noms)`** even
   before creation: `Sondes::readOn` calls `set_noms_champs_postraitables()`
   (`pb.get_noms_champs_postraitables()`) *before* reading probes, and
   `Sonde::create_champ_generique` silently skips names it does not see there.
5. **The Champ_Fonc is NOT recomputed by Postraitement itself**
   (`Champ_Generique_refChamp::mettre_a_jour` is a no-op; refChamp serves the
   referenced field as-is). All freshness must come from your `get_champ` (we
   recompute when `champ.temps() != inconnue().temps()`, i.e. once per step).

The u-dependent fields reuse the velocity infrastructure:

- `la_vitesse_transportante_->valeur_aux_elems(domaine.xp(), polys, u)` gives
  the frozen velocity at the elements (auxiliary variables of the MPFA face
  field — request them once with `init_auxiliary_variables()` + a full
  `update_ve(futur(n))` sweep, exactly like `Terme_Source_Induction_stretching`).
- `Discret_Thyd::grad_u(z, z, la_vitesse, grad)` gives the element block
  (row nf_tot + e*dim + j, col i => du_i/dx_j); the per-element physics (curl,
  Poynting, dots) lives in the framework-free `MHD_posttraitement_kernel.h`
  and is pinned by `tests/UnitTests/unit_mhd_posttraitement_kernel.cpp`.

The scripts: `syntax-check.sh` restores the MPI include dir when the recorded
compiler is an mpich wrapper (mpi.h used to be missing) — keep that.

## 11bis. Divergence of the magnetic field — Green-Gauss reconstruction and correction options

### Green-Gauss reconstruction (baseline)
The module reconstructs ∇b at element level by Green-Gauss (gradient_b_elements):
- interior face value bhat_f = centred average of the two adjacent element
  values → **exact for linear fields** on any mesh;
- border face value = the element's own value (zero normal gradient) —
  consistent with the homogeneous-Neumann CLs the module supports; probes
  must target interior cells when comparing against an analytic field.

The scalar model (nb_composantes 1) is **rejected** by creer_champ with a
clean Process::exit — ∇·b is only meaningful for a full vector b; the
kernel function mhd_tension_magnetique asserts nb_b == dim.

### Path 1 — No projection (default)
- Without `correction_divergence`, the module assumes `∇·b = 0`; this preserves
  the established equation path byte-for-byte and matches the
  thermalhydraulics velocity-field assumption pattern.
- The reported `∇b` is reconstructed by `gradient_b_elements`: centred
  Green–Gauss face values are exact for linear fields at interior cells.
  Homogeneous-Neumann wall traces use the adjacent cell value (`bf = b(e0)`),
  so boundary-cell gradients have O(h) accuracy; analytic probes should target
  interior cells.

### Path 2 — Opt-in cell-centred Hodge projection (implemented)
- Enable with `correction_divergence { }` on the magnetic equation. The
  Pass-1 divergence operator `D` uses centred interior-face averages and the
  adjacent cell value at walls. The correction uses its exact sparse transpose:
  assemble `L = D D^T`, solve `L φ = D b*`, then update `b ← b − D^T φ`.
- The operator is assembled once and uses a `1e-12` relative diagonal
  regularization for redundant rows. Projection runs during
  `preparer_calcul()` and after each time-step update; the strong-coupling
  Picard loop explicitly corrects `futur()`.
- Before projection, the code checks global boundary-flux compatibility,
  summing owned physical faces across MPI ranks. Incompatible net flux exits
  with `Gauss law violated by boundary conditions`.
- Supported boundary data are homogeneous Neumann. Non-homogeneous Dirichlet
  values are not preserved by this unconstrained projection. The projection
  enforces the module's Pass-1 measure, not every flux-divergence definition
  associated with other discretizations.
- Validation: 2D two-rank coarse/standard/fine and 3D sequential
  coarse/standard/fine scans, plus the incompatible-flux `ErrorInputs` case.
  The 3D case uses the shared active-x refinement ladder and three cells in
  each transverse direction to avoid a parity-sensitive null mode. The 2D case
  verifies MPI ghost and global-flux handling.
- The latest reportable full sweep passed 31/32 forms; `HartmannTwoWay` was
  still in progress and failed at `run.reset()` on a non-empty generated scan
  directory. The full kernel-linked unit test and sanitizer follow-ups remain
  open (arith-only tests pass).

## 11ter. Merging the post-treatment validation forms (2026-10-21)

The twelve per-field forms became **two forms, merged by dimension**:
`Postraitement_fields/Champs2D` (eleven 2D fields) and
`Postraitement_fields/Champs3D`. The move is architectural (one notebook per
dimension, shared helpers, two `parallel_ok` entries instead of twelve, two PDF
sections); the field formulas and the asserted values are unchanged in
substance.

### Why a single dataset per form is impossible
The eleven 2D fields do not share one physical state, so the 2D form carries
**two datasets** (the `MMS_2D` pattern: several `.data` templates, one
notebook, one form folder):

| Dataset | State | Why it must be like that |
|---|---|---|
| `champs2d_gradient` | `b` linear, `η = 0`, no velocity | a linear `b` is an exact equilibrium only without diffusion — and the divergence probe must include a **wall** cell, whose discrete Laplacian of a linear field is `O(1/h)`, so any `η > 0` drifts it |
| `champs2d_transport` | `b`, `u` uniform, `η = 0.25` | `nombre_de_reynolds_magnetique` divides by `η`; a uniform `b` keeps `(u·∇)b = 0` and `∇²b = 0` exactly, even at wall cells |

The same split applies in 3D (`champs3d_gradient` for the vector current,
`champs3d_transport` for the kinetic helicity, which needs a non-uniform `u`).

### Design rules that make the merged state non-degenerate
- **Non-default constants everywhere**: `mu0 2.0`, `rapport_charge_masse 2.0`,
  `mu 2.0`, `rho 4.0`, `eta 0.25`. A formula with a hard-coded `1.0` cannot
  pass. `mu0 = 1` (the default) would silently accept an implementation that
  ignores the keyword.
- **The linear field must keep both differential invariants alive**:
  `b = (x-y, x+y)` gives `∇·b = 2` **and** `(∇×b)_z = 2`. `b = (x,y)` makes
  curl/current/Lorentz vacuous; `b = (-y,x)` makes divergence vacuous.
  Consequence: `tension_magnetique` on that field is `Ft = (-y, x)`
  (`Ft_i = μ0⁻¹ Σ_j b_j ∂b_i/∂x_j` — note the index order, easy to get backwards).
- **`b` and `u` must be neither parallel nor orthogonal** in the transport
  dataset: parallel kills the Poynting vector, orthogonal kills the crossed
  helicity.

### Trap 1 — a `.son` probe stores only **9 significant digits**
Discovered when `ω_c = 2√5` read back as `4.47213595` and a `1e-12`
assertion failed. The writer is `%.9g`-equivalent, so any analytic reference
with more than 9 significant digits can never be asserted to machine
precision, no matter how exact the code is.

Fix: **choose data whose references are exactly representable**. The transport
dataset uses `b = (3,4)`, `u = (4,3)` (both of length 5 exactly) so that
`S = (14, -10.5)`, `h_c = 24`, `e_m = 6.25`, `ω_c = 10`, `Rm = 20h` are all
exact. Prefer this over loosening the tolerance — a `1e-8` tolerance would also
hide a genuine `1e-9` regression.

### Trap 2 — a `#` glued to a non-space character breaks comment toggling
TRUST tracks comment state by toggling on every `#` **that is a standalone
delimiter**. A closing `#` written flush against the text (`coverage.#`)
is not recognised, so the parity flips and the *next* line is parsed as code:

```
Error while reading '(y,' from datafile ... at line 11.
Check for missing space character.
```

Every comment line must therefore end with `" #"` (space before the final `#`).
Check mechanically before running:

```bash
python3 - <<'PY'
import glob, re
for f in glob.glob('*/src/*.data'):
    for i, ln in enumerate(open(f).read().splitlines(), 1):
        for m in re.finditer(r'\S#', ln):
            print(f, i, m.group(0))
PY
```

### Cost of the merge
One notebook run is ~4 s execute (six TRUST cases, 0.5–0.7 s of compute) plus
the PDF step, versus twelve runs before.

## 12. Open items / gotchas found

- `compile_commands.json` is stale (`c++17`); always force `-std=c++20`.
- `TRUST_Deriv` (OWN_PTR) uses `operator bool` / `.valeur()` / `.non_nul()`?
  → **non_nul() does not exist on OWN_PTR**; use `if (ptr)` and `ptr.valeur()`.
  (`OBS_PTR` has `non_nul()`.)
- `Paroi_adiabatique` app-domain compatibility is resolved through its
  complement()-typed Neumann_* class; keep domaine_application() aligned
  with the thermal domain for reuse.
- **grad_u's element rows are biased at wall-adjacent cells** (measured: the
  implied gradient is exactly halved there for a linear velocity). Consumers:
  the M1 source (§9) and `kinetic helicity` (via
  `Convection_Diffusion_Magnetic_field::gradient_vitesse_elements`); the ∇b-based
  post-treatment fields (tension, curl, divergence, current, Lorentz) go through
  the equation's own Green–Gauss `gradient_b_elements` instead and are not
  affected by this particular bias. Keep probe comparisons out of the two cells
  next to a wall; details and candidate fixes in §9.

---

## 12bis. Coupling a momentum equation: `force_lorentz` (source on Navier-Stokes)

Everything learned while wiring the low-Rm Lorentz coupling
(`src/Terme_Source_Lorentz.{h,cpp}` + optional NS equation in `Pb_MHD`).
*Note (2026-10-22): the coupled problems were restructured — `Pb_MHD` is now a
`Pb_Hydraulique` with the momentum equation at index 0 (see §20). The option
mechanism described here is unchanged; only the class holding it changed.*

- **Optional extra equations are free.** `Probleme_base::read_optional_
  equations` picks up *any* `Equation_base` subtype keyword written in the
  problem block after the main equations, discretises it and stores it in the
  protected `eq_opt_` (the mechanism of `Pb_Hydraulique`'s optional turbulence
  equations). A problem only has to expose it:
  `nombre_d_equations() { return 1 + eq_opt_.size(); }` and `equation(int)`
  mapping `i>0` to `eq_opt_[i-1]`. The NS equation keyword is
  **`Navier_Stokes_standard`** (`Implemente_instanciable_sans_constructeur`
  registers that name, not the class name `Navier_Stokes_std`).
- **Source typing carries the unknown's suffix.** `Source::typer` calls
  `Discretisation_base::get_name_of_type_for("Source", keyword, eqn)`; on the
  PolyMAC_CDO discretisation this returns `keyword + suffix("Champ")` of the
  *inconnue* class — for NS on `Polymac_p0` that is
  `force_lorentz_Face_PolyMAC_MPFA`. Register the whole family:
  `Implemente_instanciable(Terme_Source_Lorentz, "force_lorentz_Face_PolyMAC_
  CDO", Source_base)` + `Add_synonym(...)` for MPFA/HFV/unsuffixed.
  `Add_synonym` needs `#include <Synonyme_info.h>`.
- **`Source_base` runtime gates.** Override `mettre_a_jour(double)` (TRUST
  errors out at initialisation otherwise, even for a stateless source) and
  `check_multiphase_compatibility()` (empty body is fine). `associer_pb` and
  `associer_domaines` are pure virtual.
- **Momentum sources are per unit mass.** The gravity/Boussinesq precedent
  adds acceleration-like values; divide \f$\mathbf{J}\times\mathbf{b}\f$ by
  \f$\rho\f$. The assembly template for a PolyMAC face-based velocity unknown
  is `PolyMAC_family/Sources/Terme_Boussinesq_PolyMAC_CDO_Face.cpp`: face
  block `secmem(f) += (F·n_f)/S_f * vfd(f,i) * poro_face(f)` over both
  adjacent elements, plus for `Domaine_PolyMAC_MPFA` the auxiliary
  element-velocity block `secmem(nf_tot + dim*e + d) += F(d)*poro_elem(e)*
  V_e`, skipping faces with `fcl(f,0) >= 2` (Dirichlet velocity) in the face
  block. The `fcl` table lives on `Champ_Face_base` (kernel).
- **Matrix symmetry (P5, resolved).** `Terme_Source_Lorentz::ajouter_blocs`
  modifies only `secmem` (the RHS vector) — it never touches the
  matrices. The Lorentz force is an explicit, lagged source
  ($\mathbf{J}\times\mathbf{B}\f$ evaluated at $b^s$), so it
  does not enter the momentum matrix at all. Matrix symmetry is therefore
  determined by the kernel operators: the PolyMAC_MPFA diffusion operator
  is symmetric, the convection operator is non-symmetric (upwind). This is a
  kernel-level property, not a module concern. If a future implementation
  treats the Lorentz force implicitly (in the matrix), symmetry must be
  revisited — but for the current lagged scheme, P5 is a non-issue.
- **NS requires `solveur_pression`** in the equation block
  (`Param::REQUIRED` non-standard keyword of `Navier_Stokes_std`). The
  keyword is auto-prefixed: write `solveur_pression Cholesky { }` (resolves
  to `Solv_Cholesky`). The plain Sparskit `Cholesky` **segfaults** here
  (deprecated, no preconditioning of `Matrice_Morse_Sym`); use the
  PETSc-wrapped direct solver `solveur_pression Petsc Cholesky { }` as in
  `trust-code/tests/Reference/Cx/Cx.data`.
- **The repo-root executable embeds the module statically.** `trust-lime_opt`
  contains the module objects (no runtime dlopen of the module .so): after
  adding source files run `./configure` (refresh the build-dir symlinks),
  `make module_optim` (the `.so`), **and** `make exe` in
  `build/src/exec_opt` (relink `trust-lime_opt`). `make module_semi_optim`
  alone does NOT refresh the binary the `trust` script runs — the symptom is
  a freshly compiled class reported as "not a recognized keyword".

---

## 13. Writing self-contained TRUST test cases (no `.med` needed)

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

## 14. Validation workflow

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
dimensions. Scope is 2D *and* 3D: every form exists in both
`MagneticStandalone/` and `MagneticStandalone3D/` (the 3D twins keep the same
assertions). For each decision: is it pinned by a test, and where?

| # | Modelling decision | Pinned? | By |
|---|---|:--:|---|
| 1 | **No $\rho C_p$ factor.** The equation is magnetic, not thermal; the time factor must be exactly 1. This is the single most dangerous inherited decision (the base class is the *energy* equation). | ✅ | `diffusion`, `diffusion_refined` — the *absolute* decay rate $e^{-\eta\pi^2t}$ pins the constant; a $\rho C_p$ factor would be off by ~10³ |
| 2 | **η is used verbatim** as the diffusion coefficient (not a transformed quantity). | ✅ | `diffusion` (rate), `steady_state` (divergence form) |
| 3 | **η is used for the operator *and* for the time-step estimate** (`diffusivite_pour_transport` / `..._pour_pas_de_temps` both return η). | ⚠️ partial | only indirectly (a wrong value changes dt, not the converged solution) |
| 4 | **Components are transported independently** — no coupling, no projection. | ❌ **not testable today** | the module is scalar-only (`nb_composantes = 1`); the multi-component vector case is deferred with M3. Two distinct eigen-rates would pin it — see the `vector_components` note below |
| 5 | **No spurious source**: equilibria are preserved exactly. | ✅ | `invariance` (1e-10), `steady_state` (1e-8) |
| 6 | **Diffusion is in divergence form** $\nabla\!\cdot\!(\eta\nabla b)$, so a spatially varying η behaves correctly. | ✅ | `steady_state` ($\eta=1+x$, exact equilibrium) |
| 7 | **Velocity is given data and is frozen**; no feedback from $b$ to $u$. | ✅ | `advection`, `varying_velocity` |
| 7b | **M1 source $S=(b\cdot\nabla)u$** has the right magnitude, sign and gradient-index order (scalar transport). | ✅ | `source_stretching` ($b=B_0e^t$) |
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
| `advection` | **no** — convection-operator test only |

(The former `vector_components` form — a machinery test for multi-component
handling — was removed: the scalar-only module cannot run it. Its two-distinct-
eigen-rates idea is the natural test once vector `nb_composantes` lands.)

If Gauss-law correctness ever becomes a requirement, it needs an M3-style
projection or a vector-potential formulation, plus **per-component boundary
conditions**. Both are deferred (see `docs/content/model.md` §6).

Useful technique: give two components or two runs **different** analytic
behaviour (different eigen-rates, coarse vs refined mesh) so a plumbing bug
cannot hide behind symmetry.

## 15. Running a validation case in parallel (`trust -partition`)

Upstream mechanism (kernel `Validation/Outils/trustutils/jupyter/run.py` +
`bin/KSH/make_PAR.data`), used by every ported trust-LiMe form:

- `run.addCase(".", "case.data", nbProcs=2)` + `case.partition()` +
  `run.runCases()`. `TRUSTCase.partition()` renames the dataset to
  `PAR_<case>` and shells out to `trust -partition <case> 2`; the generated
  exec script runs `trust PAR_<case> 2` (-> `mpirun -np 2`). In trustutils'
  default *sequential-suite* mode this still works: `nbProcs` sets the number
  of **ranks per case**, not suite concurrency.
- `make_PAR.data` builds `SEQ_`/`DEC_`/`PAR_` variants by toggling the
  comment markers: nominal = mesh active + partition/scatter commented;
  `DEC_` = partition un-commented (runs `trust DEC_<case>` -> writes
  `<zones>_000%.Zones`); `PAR_` = mesh commented + `Scatter` active. It also
  rewrites `Nb_parts`/`Tranches` to the requested count, so the notebook's
  `nbProcs` is authoritative.
- Dataset requirements: `# BEGIN MESH # ... # END MESH #` around the mesh
  generation, a `# BEGIN PARTITION ... END PARTITION #` block
  (`Partition_tool metis { nb_parts 2 } Larg_joint 2 zones_name DOM`), and a
  `# BEGIN SCATTER ... END SCATTER #` block (`Scatter DOM.Zones dom`).
  Upstream examples: `Verification/SampleFormJupyter/src/Couette/Couette_cylindrique.data`,
  `Verification/HowTo/Post_treatment_performance/src/Obstacle.data`, and for
  3D PolyMAC_MPFA `Verification/PolyMAC_MPFA/grad_rot/src/grad_rot.data`.

Traps (all hit for real during the 2026-10-16 parallel port):

- After `partition()`, the dataset and **every** output file are named
  `PAR_<case>` - read probes as `read_son(case.dataFileName_, ...)`. TRUST
  also upper-cases sonde ids in probe file names: sonde `sonde_Bx` writes
  `..._SONDE_BX.son` - the `read_son` argument must be upper case.
- `#` is a **token-level open/close pair** in the data-file lexer
  (`src/Kernel/Utilitaires/LecFicDiffuse_JDD.cpp`), not a line comment. A
  `# ... #` line inside a region that `make_PAR.data` turns into a
  skip-region (the MESH / PARTITION blocks) closes the skip early and
  desynchronises the whole file - use `/* ... */` there instead (as upstream
  `Obstacle.data` does).
- In 3D, `Polyedriser dom` goes **inside** `# BEGIN MESH # ... # END MESH #`:
  the polyhedra must come from the pre-partitioned `.Zones`, not be
  re-derived in the PAR run.
- **`make_PAR.data` rewrites solver lines by grepping the whole file.** Its
  "Cholesky is not parallelized" rewrite fires when *any* line contains
  `Cholesky` without `PETSc` - **including `# ... #` comment lines**. It hit
  `ForceLorentzBalance`: the comment `# direct Cholesky is deprecated ... #`
  triggered the sed, mangling `solveur_pression Petsc Cholesky { }` into
  `Petsc GCP { precond ssor ... }`, which diverged with
  `KSP_DIVERGED_NANORINF`. Rule: every comment line that mentions Cholesky
  must also mention PETSc on the same line (upstream's own check comment:
  *"Cholesky par Petsc est parallele"*).
- **The repo-root executable embeds the module statically** (see 12bis):
  after touching module sources, `make module_semi_optim` / `module_optim`
  rebuild only the `.so` - **relink the exec** (`cd build/src/exec_opt &&
  make exe`, or the semi_opt twin) or every run keeps using the old code.
  The gradient fix below was silently inert until the relink was done.
- `Convection_Diffusion_Magnetic_field::gradient_b_elements()` fills `gb`
  for **all elements, local and ghost** (`nb_elem_tot()`): parallel consumers
  (the Lorentz source) evaluate per-element data on ghost elements through
  joint faces, so ghost rows must be in-bounds and filled (the pre-fix code
  sized `gb` to `nb_elem()` - an out-of-bounds read on every ghost element,
  benign sequentially where the two are equal). Ghost elements at the edge of
  the ghost window have degraded gradients (their outward faces point
  outside the window, `face_voisins = -1`, treated as one-sided) - harmless,
  because joint faces only involve first-layer ghosts, whose gradients are
  exact (verified: local max error 8e-15; parallel `dp` identical to
  sequential to all digits on ForceLorentzBalance).
- Gate: `check-validation.sh` R2/R3 forbid parallelism by default; the ported
  forms are listed in `parallel_ok[]` (path prefixes). Extend that list
  deliberately - never disable R2/R3. For allowlisted datasets the gate
  additionally requires the three marker regions to be present.

## 16. Traps hit while adding the coupled-MHD validation forms (2026-10-17)

*(Status 2026-10-19: `MMS_CoupledRefined` was absorbed into
`MMS_Coupled`'s mesh-convergence scan at merge M2, `TwoWayFeedback` was
folded into `Coupled_Feedback` at merge M3 — the contracts they carried
are unchanged. Status 2026-10-20: the four coupled error forms
`Error_BadForceKeyword`/`Error_MissingPressureSolver`/`Error_TwoWayNoNS`/
`Error_TwoWayBothVelocities` were merged into `MagneticCoupled/ErrorInputs`
— same datasets, same per-case asserts and print lines, verified
line-identical against the historical executed outputs.)*

*(Status 2026-10-21: `MMS_Coupled3D` was folded into `MagneticCoupled/MMS_Coupled`
as a second dataset — the form now runs its 2D case and its 3D twin from one
notebook; see §19. The `MagneticCoupled3D` category stays, holding only the
in-progress `ShercliffDuct3D`.)*

Seven forms added (`MMS_Coupled`, `MMS_CoupledRefined`, `Coupled_Feedback`,
`MMS_Coupled3D`, `Error_BadForceKeyword`, `Error_MissingPressureSolver`,
plus the `ForceLorentzBalance` hardening). Findings:

- **nbformat requires `outputs` and `execution_count` on code cells.** A
  notebook built by a script (plain dicts with `cell_type`/`id`/`metadata`/
  `source`) fails at `nbconvert --execute` with
  `NotebookValidationError: 'outputs' is a required property` — jupyter
  validates before running. Always emit `"outputs": []` and
  `"execution_count": None` on every code cell.
- **`Champ_Fonc_xyz dom N …` — N is the component count, not the dimension.**
  With `nb_composantes 2` in a 3D problem, the field is
  `Champ_Fonc_xyz dom 2 y -x` (two expressions); writing `dom 3 y -x 0`
  segfaults TRUST at the initial-condition read.
- **Boundary keywords: `droite`, never `droit`.** The Pave block registers
  `Bord droite …`; a BC line `droit Neumann_Paroi_adiabatique` (missing 'e')
  crashes with `rang_frontiere(): we have not found a boundary with name`.
- **`TRUST_NB_PROCS` leaks from the shell environment** into the TRUST
  binary and forces parallel mode — a sequential form then dies with
  "Mailler keyword can't be used in parallel calculation!". The existing
  `Error*` forms exploit this: they carry the parallel markers and their
  notebooks run `trust CASE 2`, failing cleanly (rc=255) by convention.
- **`make_PAR.data` Cholesky rewrite: never write a comment that says
  "Cholesky" without "PETSc" on the same line** — even a NOTE explaining
  the trap triggers it. Keep solver comments free of the word entirely.
- **Point-probe convergence studies are unreliable in the coupled case**:
  the discretisation error is spatially varying and non-monotonic, and probe
  snapping (cell centres shift by h/2 under 2× refinement) contributes an
  O(h²) offset comparable to the truncation error. `MMS_CoupledRefined`
  therefore documents mesh independence (bounded error) rather than a
  convergence order. (Absorbed into `MMS_Coupled`'s scan at merge M2 —
  the scan asserts the baseline's 5 pct bound at every level.)

---

## 17. Two-way coupling + mesh-convergence scans + notebook merging (2026-10-17→19)

Lessons from the two-way rollout (`vitesse_hydraulique`) and the
mesh-convergence/notebook-merging session. The geometry *at that time*:
Pb_MHD held the magnetic equation as equation 0 and the optional
`Navier_Stokes_standard` was created, discretised and read *after* the
magnetic block (`Probleme_base::read_optional_equations`). Since §20 (2026-10-22)
the momentum equation is a first-class equation at index 0 of `Pb_MHD`; the
`couplage_fort` behaviour described below is unchanged.

- **`Operateur_Conv` is a two-hop velocity binding.** The wrapper's
  `associer_vitesse()` only stores its own `la_vitesse` observer
  (`Operateur_Conv.h:75-78`); the forward to the underlying operator
  happens **once, in `Operateur_Conv::typer()`** (`Operateur_Conv.cpp:80`,
  read time, with whatever velocity `vitesse_pour_transport()` returns
  then — the zero placeholder). Any post-read rebind of the convection
  operator must go through `terme_convectif.valeur().associer_vitesse(...)`
  (`Operateur_Conv_base` pure virtual) — a wrapper-only call is a silent
  no-op for the evaluator (this is what kept `b` frozen in the first
  two-way draft until the TwoWayFeedback calibration caught it).
- **Late binding point**: `completer()` — runs once every equation is known
  (`Probleme_base::readOn` step 4); the magnetic equation's own
  `completer()` rebinds the equation-level pointer AND the operator.
  Anything consuming velocity must use the base
  `vitesse_transportante()`, never the module's placeholder member
  `la_vitesse_transportante_` — they diverge after the rebind
  (post-treatment serving zeros otherwise).
- **Time-slot semantics** (why the scheme is fully lagged):
  `Schema_Euler_Implicite::faire_un_pas_de_temps_eqn_base` restores
  `present = passe` at each equation's step (so during the NS step the
  magnetic `valeurs()` is still bˢ), and the wheel rotates only at
  `validateTimeStep` (`Probleme_base::mettre_a_jour`, index order).
  Sources are updated *before* the NS inconnue rotates, so a source-level
  rebuild lags one step behind the convection term — rebuild lazily at
  assembly time instead (keyed on the velocity's own time: one rebuild
  per step).
- **`lire_sources` uses a process-global `static bool already_read`**
  (`Equation_base.cpp:327-342`) with `Process::exit()` commented out: a
  *second* `sources` block prints a cosmetic `Error: ... should only appear
  once per equation` and continues. Cosmetic-only, but do not "fix" the
  data file.
- **`Equation_base::lire_sources` is per-equation**, so merged multi-case
  notebooks are fine.

### Mesh-convergence scan convention (2026-10-19, renamed 2026-10-20)

- **Single template**: `src/<case>.data` **is** the trustutils template —
  `$nx $ny [$nz]` placeholders in the `Nombre_de_Noeuds` line (partition
  markers kept — allowlisted datasets stay parallel-portable). No
  `.tpl.data` twin any more: every scale runs the SAME file, substituted
  per scale via `run.addCaseFromTemplate` → `string.Template`.
- **Scale naming** (never `L1`/`L2` — confusable with the L¹/L² metric
  spaces): `coarse` = the baseline mesh, `standard` = cells ×2
  (`2N−1` nodes), `fine` = cells ×4 (`4N−3`), only in the ACTIVE
  directions (freeze passive ones: Hartmann/Shercliff refine y only;
  MMS_Diff3D y only — a single-mode eigenmode is 1D).
- **Per-scale build dirs** `build/<scale>/` (merged multi-case forms:
  `build/<Case>/<scale>/`) — `trust -partition` writes `DOM_000*.Zones`
  in the case's directory, so cases sharing a directory overwrite each
  other's zones (see §18 — the M4-era MMS baselines shipped with exactly
  that bug).
- **Probe reading after `partition()`**: a case's sondes are named
  `PAR_<case>_SONDE_*`; read via `case.dataFileName_` (the case object),
  never a literal name. `read_son()` looks in `build/coarse/` (and
  `build/<Case>/coarse/`) first, then the build root (plain `addCase`
  datasets, e.g. the twoway twin).
- **Identity-scan contract**: machine-precision bounds asserted at every
  scale; for position-dependent analytics, evaluate the analytic at the
  probe's EFFECTIVE cell-centre position from the `.son` header
  (`# Temps x= ... y= [z= ...]`, tokens after normalising `=` — note the
  token after `x` is `=`, the value is at +2). `nombre_de_reynolds_magnetique`
  L_e is the CELL size — scale-dependent expected value.
- **Volatile outputs in the RULE harness**: nb-compare skips `tablePerf`
  cells and must also strip the trustutils timing line
  (`=> A total of N cases ... run in X.Xs`) — run cells whose source
  didn't change are otherwise output-compared (fixed 2026-10-19). Cells
  whose source changed are paired by cell index; their stream text is
  compared after mapping the historical `L0`/`L1`/`L2` labels to
  `coarse`/`standard`/`fine` and collapsing column-alignment whitespace
  (the longer labels change table padding — presentation, not
  computation); their images are not compared (plot legends may carry the
  renamed labels) — the physics stays covered by the `.son` comparison.
- **Scan metrics**: exact-implicit-Euler-cancelled decay
  (temporal error is common to every mesh, so the residual is the spatial
  one); wall-profile core errors (wall-adjacent Green–Gauss bias
  documented and excluded); steady identities (drift + per-scale P0
  projection bound); temporal-floor cases (SourceStreching,
  hydrostatic Δp): fixed bound at every scale, no monotone order.

---

## 18. Two latent bugs caught by the RULE harness during the 2026-10-20 scan rename

The rollout to the coarse/standard/fine naming (single substituted template,
per-scale build dirs) was RULE-gated per form. Two findings were NOT
presentational and are recorded here.

### 18.1 Build-root `DOM_000*.Zones` collision (merge-era, real bug — now fixed)

`trust -partition <case>.data N` writes `DOM_0000.Zones`/`DOM_0001.Zones`
**in the case's directory**. Several partitioned cases registered in the
SAME directory therefore overwrite each other's zones, and every solve
afterwards reads the zones of the LAST partition — i.e. it solves a
DIFFERENT MESH than its own data file.

- The M4a/M4b-merged `MMS_2D`/`MMS_3D` notebooks shipped with exactly this
  bug: their four baselines were registered at the build root, so all
  solves used the last-partitioned baseline's mesh. Confirmed on the
  reproduced tree: every `PAR_MMS_*.err` reports "Total nb of elements =
  1024" (= the 33×33 `MMS_Poly_2D` mesh) although `MMS_Diff2D.data` says
  21×21 nodes (400 cells); the recorded probe positions (0.484375 =
  15.5/32) and the mesh-dependent `dt_stab` grids match the 33×33 mesh,
  not each case's own. The MMS_2D ExpTrig baseline checks therefore
  passed "perfectly" on the wrong (finer) mesh, and the MMS_3D Diff3D
  baseline ran on a 5×5×5 mesh instead of its 21³ (its scan table even
  showed a bogus order 6.63 for the baseline level).
- Why nobody noticed: the invariance identities hold on any mesh, and the
  decay checks have generous bounds. The suites were green since 2026-10-18.
- Fix: the per-scale (and per-case) build subfolders of the new layout give
  every case its own `DOM_000*.Zones`. The MMS baselines now run their own
  meshes; the MMS_3D Diff3D scan table now shows a clean order ~2 across
  all three scales. The nb-compare "NOT VALIDATED" verdicts on the two MMS
  forms during the rollout flagged exactly these (deliberate, documented)
  changes; every other diff was the Poly template sondes now present in the
  baseline file (single-file convention).
- Rule of thumb: **one partitioned case per directory.** A plain
  `addCase(".")` case may share the build root only with cases of the SAME
  mesh (e.g. Coupled_Feedback's twoway twin — that is why the M3 merge never
  tripped this).

### 18.2 Layout-dependent SIGFPE in the parallel path (post-treatment 3D fine scale — guarded)

**2026-10-22 — defensive guard applied in the kernel.** `Op_Diff_PolyMAC_MPFA_Elem::calculer_dt_stab()`
now skips (a) face contributions whose `volumes_entrelaces(f)` is denormal/zero and
(b) flux entries whose magnitude is below `1e-30` (the `flux(n)` truthiness guard used to
pass denormals, causing `1/flux` to overflow to FPE). The Champs3D fine scale now runs
cleanly on 2 ranks (`Finished correctly`). The underlying uninitialized read in the kernel's
parallel init path (porosite/volumes_entrelaces/joint-face data) is **not** pinned; the guard
makes the code robust to it but the root cause remains open for a future full-kernel ASan pass.

The form that first exposed it was the pre-merge
`Postraitement_fields/CourantMagnetique`, fine scale (17³ nodes), case name
`PAR_courant_magnetique`, `nbProcs=2`: 100% reproducible crash at problem
initialisation (`signal 8 / FPE_FLTINV` right after the Fluide initialiser,
between `Domaine_VF::marquer_faces_double_contrib` and the solve banner).

Facts established by bisection (identical data bytes, identical
`DOM_000*.Zones` md5s in every experiment):

| case name | directory | result |
|---|---|---|
| `PAR_courant_magnetique` | `build/fine/` (the form) | FPE, 100% (4/4 runs) |
| `PAR_courant_magnetique` | scratch copy | FPE |
| `PAR_courant_magnetique_L2` (the historical name) | scratch copy of `build/fine/` | FPE |
| `PAR_courant_magnetique2`, `PAR_ccm`, `PAR_jdd` | same scratch copy | clean, correct values |
| `PAR_courant_magnetique` | sequential (`nbProcs=1`) | clean, correct values |

So the crash depends on the case-name/path STRING (heap layout), not on any
physical or data content: the classic signature of an uninitialized-memory
read somewhere in the TRUST parallel init path (kernel or module — the
`Champ_Fonc_MHD`/`gradient_b_elements` code was audited and is
bounds-checked with its buffers zeroed). The historical `scan/L2` run of
the same mesh passed purely by layout luck.

Interim handling (visible, not hidden — see the form's run cell): the fine
scale of that form ran on 1 processor; the sequential probe values
reproduced the historical L2 values exactly (the RULE compare was green).
**The underlying memory bug must be pinned by the sanitizer pass
(ASan/UBSan, `make module_custom`)** — until then any layout shift
(name, path, build) can flip the crash on or off for any form.

**2026-10-21 — the crash moved away with the post-treatment merge.** That form
was replaced by `Postraitement_fields/Champs3D` (dataset `champs3d_gradient`,
same 17³ fine scale, `nbProcs=2`, longer case name): 3/3 fine runs plus every
coarse/standard run are clean, with machine-precision values, so the merged
form carries **no** rank pin. This is exactly the predicted layout-luck
behaviour — the kernel bug is still open, so re-verify with the sanitizer pass
before relying on any particular name/path (and if a future rename flips it
back on, reapply a local `nbProcs=1` for that scale rather than weakening the
probe).

**Sanitizer-pass findings 2026-10-20** (module-instrumented ASan/UBSan
build + gdb):

- Crash site localised: `Op_Diff_PolyMAC_MPFA_Elem::calculer_dt_stab()`
  (kernel), from `Schema_Temps_base::initialize()` →
  `Probleme_base::calculer_pas_de_temps()` → `Operateur::calculer_pas_de_temps()`.
  FPE_FLTOVF: the `dt = min(dt, pe(e)*ve(e)*(lambda/diffu)/flux(n))` term
  overflows because `flux(n)` is a **denormal** (~1e-316 — decoded from the
  xmm1 divisor register at the faulting `vdivsd`): nonzero, so the
  `flux(n)` guard passes, tiny enough that 1/flux overflows.
- `flux` = sum over the element's faces of `nu_dot(&nu_, e, n, nf(f), nf(f))
  / vf(f)`; `update_nu()` audited — it writes ALL rows to `nb_elem_tot`,
  so the garbage is NOT in `nu_` itself but in one of the other reads
  (porosite/volumes_entrelaces/joint-face data) — all kernel-side.
- Module-only ASan does NOT fire on the crash → the uninitialized read is
  in kernel code (uninstrumented in a module_custom build).
- gdb heisenbug confirmed: with ASLR disabled (`set disable-randomization
  off` missing) the run is clean; re-enabling randomization reproduces the
  FPE — memory-layout dependence, verified at binary level.
- Recipe for sanitizer builds (2026-10-20, works around two baltik gotchas):
  1. `project.cfg` `ld_flags` CANNOT take `-fsanitize=...` — baltik parses
     each ld_flags token via `find_library(<token> NAMES lib<token>.so ...)`
     and the `=` breaks the CMake variable name. Keep project.cfg clean.
  2. Create `$TRUST_ROOT/env/Cmake.custom` (modeled on Cmake.env) setting
     `CMAKE_CXX_FLAGS_CUSTOM` / `CMAKE_C_FLAGS_CUSTOM` /
     `CMAKE_EXE_LINKER_FLAGS_CUSTOM` with
     `-fsanitize=address,undefined -fno-omit-frame-pointer -O1 -g -Wno-error`.
  3. Third-party libs have no `_custom` builds: symlink
     `linux_custom -> linux_opt` in `$PETSC_ROOT`, `$TRUST_KOKKOS_ROOT`,
     `$TRUST_MEDCOUPLING_ROOT`.
  4. Module-only instrumentation: also symlink
     `lib/TRUST_mpi_custom.a -> TRUST_mpi_opt.a` (+ `_gfort`/micro variants)
     then `./configure && make module_custom` (minutes); the binary is
     `trust-lime_custom`.
  5. Full-kernel instrumentation: run `make custom` in the TRUST checkout
     (builds TRUST_mpi_custom.a), then `make custom` in the module —
     hours, but the only way to instrument kernel-side reads.
  6. `ASAN_OPTIONS=handle_sigfpe=2` makes ASan print a symbolised stack
     for kernel FPEs even in partially instrumented builds — that is how
     the crash site was localised; `detect_leaks=0` (TRUST's exit path is
     not leak-clean by design).


### 18.3 The coupled error path segfaults after printing its diagnostic (open)

All four coupled malformed-input datasets print their diagnostic and then
die with **SIGSEGV (`rc=139`)** instead of a clean non-zero exit
(`rc=255`, like the standalone error forms). Pre-existing since the forms
were created (the historical notebooks show the same `rc=139` tails — the
bash wrapper's crash report, which embeds the volatile PID, is what lands
in the `tail[0][:90]` print line); the merged `ErrorInputs` form keeps the
behaviour and its asserts treat any non-zero exit as the pass condition.

**Sanitizer-pass finding 2026-10-20**: under the module-instrumented ASan
binary the diagnostic prints COMPLETELY ("INVALID_KEYWORD not understood
by force_lorentz_Face_PolyMAC_CDO ... TRUST caused error and will stop")
and the segfault happens AFTER it — inside the kernel's own error-exit
path (`Process::exit` after the interpreter error), not in the module
(no ASan report; the path is uninstrumented). Pure-kernel defect, worth an
upstream report regardless of what the full-kernel ASan build adds.

## 19. Folding a 2D form and its 3D twin into one notebook (2026-10-21)

`MagneticCoupled3D/MMS_Coupled3D` was retired: its dataset
(`mms_coupled_3d.data`) moved into `MagneticCoupled/MMS_Coupled/src/` and the
2D notebook grew a 3D section, so the `MMS_Coupled` form now runs **two
datasets, two ladders**. Recipe and traps:

- **One build subfolder per dataset is mandatory.** `trust -partition` writes
  `DOM_000*.Zones` **into the case directory**, so two cases sharing a
  `build/<scale>/` target silently overwrite each other's partition (every
  solve then runs on the last partition's mesh). Register every case as
  `run.addCaseFromTemplate("<case>.data", "<case>/<scale>", ...)` →
  `build/<case>/<scale>/` (the `MMS_2D` pattern; the merged notebook keeps a
  flat-layout fallback in `son_path()` so a single-case form still works).
- **The `.son` file carries the `PAR_` prefix**: after `partition()`,
  `case.dataFileName_` is `PAR_<case>`, and the probe file is
  `build/<case>/<scale>/PAR_<case>_<sonde>.son` — resolve the subdirectory
  from the un-prefixed name.
- **Keep the per-geometry cells byte-identical.** Cells that did not change
  keep their verbatim source (only the local alias `case` → `case_2d` /
  `case_3d`), and the duplicated helpers (`read_son_all`, `read_son_dir`,
  `probe_pos`) move into one shared helpers cell. The RULE harness
  (`notebook-baseline.sh execute|compare`) then sees identical printed output.
  Name the per-geometry scan helpers apart (`dp_error_2d` / `dp_error_3d`):
  two same-named definitions in one notebook shadow each other with
  *different* dimensionality and read like a bug.
- **`notebook-baseline.sh execute` must run BEFORE the source notebook is
  deleted** — the 3D snapshot is the only reference for the 3D prints.
- **The probe positions snap to cell centres under refinement** (3D:
  `0.375^3 → 0.3125^3` at `standard`, `0.34375^3` at `fine`), so a
  mesh-convergence metric **must** read the effective position from the
  `.son` header (`# Temps x= … y= … z= …`) and evaluate the analytic
  reference there; the nominal position would carry an O(h) error that is
  not a physics error.

### 19.1 The 3D MMS baseline reference is 2D-shaped (known sharp edge)

The historical 3D baseline declared
`expected = -(0.625**2 + 0.625**2 - 0.375**2 - 0.375**2) = -0.50` — a
**two-term, 2D-style** expression — while the 3D probes sit on the diagonal
`(0.375,0.375,0.375)` and `(0.625,0.625,0.625)`, where the exact drop of
`p = -(x^2+y^2+z^2)/mu0^2 rho` is `-0.75`. The check passes only because its
bound is 50 %: the coarse mesh measures `-0.744671`, i.e. **48.9 %** away
from `-0.50` — about 2 % of margin left. The tight statement about the 3D
geometry is the *scan* cell, which uses the correct three-term expression at
the effective positions (`0.7 % / 0.2 % / 0.3 %` at coarse/standard/fine).

The merge deliberately keeps that baseline cell **verbatim** (contract
unchanged). If a mesh or solver change ever trips it, the fix is one line:
use the three-term expression and the same 5 % bound as the 2D case
(measured margin then 7×). Do **not** widen the 50 % bound further.

### 20. Strong (Picard) coupling — opt-in `couplage_fort` (2026-10-22)

`Convection_Diffusion_Magnetic_field` now accepts three optional keywords (registered in
`set_param`, read when the equation block is parsed):

- `couplage_fort` (bare flag) — enable Picard iteration of the magnetic <-> momentum
  sweep inside a time step (two-way mode only; ignored otherwise).
- `nb_iterations_couplage <n>` (default 5) — max sweeps per step.
- `tolerance_couplage <eps>` (default 1e-8) — relative-change convergence threshold.

When enabled, `Pb_MHD::iterateTimeStep` repeats the Gauss-Seidel sweep up to `n` times
or until both the magnetic field and the velocity change by less than `eps`. The sources
read the latest (`futur`) values in this mode: `gradient_b_elements`, `Terme_Source_Lorentz`
and `Terme_Source_Induction_stretching` switch to `inconnue().futur()` / rebuild grad(u)
every sweep. Default (flag off) preserves the historical fully-lagged staggered scheme, so
the 27-form green baseline is untouched. The convection operator still reads the
present slot, so this is a semi-strong coupling (Lorentz/stretching contributions are
iterated, transport uses the time-level velocity); true monolithic coupling remains
future work.

Accessors: `eq.couplage_fort()`, `eq.nb_iterations_couplage()`, `eq.tolerance_couplage()`.

## Induction turbulence (turbulent magnetic diffusivity)

The induction equation accepts an optional `diffusivite_turbulente_magnetique`
keyword (a `Champ_Don_base`, i.e. any field — `Champ_Uniforme`, `Champ_Fonc`,
`Champ_Inc`, ...) representing the eddy diffusivity \f$\eta_t\f$. It is folded
into the molecular diffusivity so the equation runs on \f$\eta_{\rm eff} =
\eta + \eta_t\f$:

- **Phase 1 (uniform):** if both \f$\eta\f$ and \f$\eta_t\f$ are `Champ_Uniforme`,
  `readOn` sums them into a single `Champ_Uniforme` stored in
  `diffusivite_effective_` (message `turbulent magnetic diffusivity folded,
  eta_eff = ...`).
- **Phase 2a (non-uniform):** otherwise `completer()` builds a
  `Champ_Fonc_Somme` (`src/Champ_Fonc_Somme.{h,cpp}`) that evaluates
  \f$\eta_{\rm eff}(x) = \eta(x) + \eta_t(x)\f$ point-wise and stores it in
  `diffusivite_effective_`. The operands are observed (`OBS_PTR`) so a
  \f$\eta_t\f$ that is itself a `Champ_Fonc` (e.g. derived from \f$k,\varepsilon\f$)
  is tracked automatically.
- `diffusivite_pour_transport()` / `diffusivite_pour_pas_de_temps()` return
  `diffusivite_effective_` when set, else `diffusivite_magnetique_`.

Phase 2b (transport equations for \f$k,\varepsilon\f$ and the realizable
\f$C_\mu k^2/\varepsilon\f$ closure, following TrioCFD's two-layer k-\f$\varepsilon\f$
method) is still open: the current seam only provides the *interface*; the
turbulence model itself is not yet implemented.

### Phase 2b — turbulent magnetic diffusivity from k, epsilon

In addition to accepting a ready-made `diffusivite_turbulente_magnetique` field,
the equation can build it from two operand fields:

- Keywords: `k_magnetique <field>`, `epsilon_magnetique <field>`,
  `C_mu_magnetique <float>` (default 0.09), `prandtl_turbulent_magnetique <float>`
  (default 1.0).
- In `completer()`, when both `k_magnetique_` and `epsilon_magnetique_` are set and
  `diffusivite_turbulente_magnetique_` is not already given, a
  `Champ_Fonc_Eta_Turbulent` (`src/Champ_Fonc_Eta_Turbulent.{h,cpp}`) is built:
  `eta_t(x) = C_mu * k(x)^2 / eps(x) / Pm_t`, with a floor on eps (1e-12).
  It is stored in `diffusivite_turbulente_magnetique_` and then folded by the
  Phase 2a logic (uniform -> Champ_Uniforme sum at read time; non-uniform ->
  Champ_Fonc_Somme point-wise). Operands are observed (OBS_PTR) and refreshed in
  mettre_a_jour, so a k/eps that is itself a Champ_Fonc is tracked.

Realizable C_mu and wall limiters are deferred (Phase 2b+).

## Phase 3 — independent magnetic k-epsilon turbulence (implemented in source, validation pending)

The independent two-equation magnetic-fluctuation closure is now implemented
in the source tree (commit `d83a076`), wired through the
`modele_turbulence_magnetique { ... }` block of
`Convection_Diffusion_Magnetic_field` and the optional extra equations of the
problem (kernel `eq_opt_` mechanism). It is mutually exclusive with the
prescribed `diffusivite_turbulente_magnetique` and `k_magnetique`/`epsilon_magnetique`
providers.

- `Modele_turbulence_magnetique` (`src/Modele_turbulence_magnetique.{h,cpp}`) —
  owns the closure coefficients (`C_mu_b`, `C_eps1b`, `C_eps2b`,
  `sigma_kb`, `sigma_epsilonb`, `Pm_t_b`), floors/caps, the Alfvén
  normalization `field_scale = 1/sqrt(mu0*rho_ref)`, the derived `eta_t`
  (`Champ_Fonc_Eta_Turbulent`), and the k/epsilon diffusivities
  (`Champ_Fonc_Somme`, `eta + eta_t/sigma`). It locates the two transport
  equations inside the hosting problem and calls their `associer_modele`.
- `Convection_Diffusion_K_Magnetic`, `Convection_Diffusion_Epsilon_Magnetic`
  (`src/Convection_Diffusion_*_Magnetic.{h,cpp}`) — real scalar transport
  equations (inherit `Convection_Diffusion_Temperature_base`), with their own
  unknowns `k_b`/`epsilon_b`, diffusion/convection operators bound to the
  induction velocity, and an auto-added source term. `vitesse_pour_transport()`
  and `diffusivite_pour_transport()` are rebound to the magnetic equation's.
- `Terme_Source_K_Magnetic`, `Terme_Source_Epsilon_Magnetic`
  (`src/Terme_Source_*_Magnetic.{h,cpp}`) — production/dissipation sources:
  `P_b = eta_t * |curl(b_A)|^2` (Alfvén-normalized), `k` source
  `P_b - epsilon_b`, epsilon source `C_eps1b*(epsilon_b/k_b)*P_b -
  C_eps2b*epsilon_b^2/k_b`.

Status: the code builds and the keyword is accepted, and an experimental
smoke dataset exists at
`share/Validation/Rapports_automatiques/MagneticStandalone/MagneticTurbulence/`.
That case still aborts at `Solve` (under investigation); the closure is
**not** yet validated. No wall functions, low-Re corrections or restart
handling are implemented, and the production term uses the cell-centred
Green-Gauss curl (consistent with the induction operator).

## Phase 4 (hydraulic k-eps coupling)

TrioCFD is now a baltik dependency (`project.cfg` `[dependencies]`). In
`completer()`, when `vitesse_hydraulique_` is set and no explicit k/eps fields
are given, the magnetic equation checks the NS equation's turbulence model
(`Navier_Stokes_Turbulent::modele_turbulence()`) for a `K_Eps` (or separate
`k`/`epsilon`) field and builds the `Champ_Fonc_Eta_Turbulent` from it
(component 0 = k, component 1 = eps for the 2-component K_Eps field).

## 20. Problem hierarchy: `Pb_Induction` / `Pb_MHD` / `Pb_MHD_Turbulent` (2026-10-22)

The single `Pb_MHD` (a `Probleme_base` whose equation 0 was the magnetic
equation, with the momentum equation bolted on as an *optional* extra) is
split the way the kernel splits its own problem family:

| Class | Base | equation(0) | equation(i>=2) |
|---|---|---|---|
| `Pb_Induction` | `Probleme_base` | magnetic | — |
| `Pb_MHD` | `Pb_Hydraulique` | `Navier_Stokes_std` | `eq_opt_[i-2]` |
| `Pb_MHD_Turbulent` | `Pb_Hydraulique_Turbulent` | `Navier_Stokes_turbulent` | `eq_opt_[i-2]` |

Why `Pb_Induction` is **not** a `Pb_Fluide_base`: several kernel source terms
treat `sub_type(Pb_Fluide_base, pb)` as the contract "equation(0) is a
`Navier_Stokes_std`" and `ref_cast` it unconditionally
(`VDF/Sources/Terme_Source_inc_VDF_Face.cpp`,
`Terme_Source_Coriolis_VDF_Face.cpp`,
`Terme_Source_inc_th_VDF_Face.cpp`). The kernel's own induction-only problem
(`Pb_Conduction`) derives from `Probleme_base` for the same reason.

**Equation index vs data-file order.** `Probleme_base::lire_equations` matches
each equation keyword by *name* (`getset_equation_by_name`), so the two blocks
can be written in either order. What is fixed is the **index**: `equation(0)`
is the momentum equation (the medium is discretised through it,
`typer_lire_milieu`, `ns_ou_cond_eq = 0`) and `equation(1)` the magnetic one.
The momentum block is written first by convention and every validation dataset
does so. A turbulent momentum equation has to stay at index 0 for the same
reason.

**Medium association.** `Pb_Hydraulique::associer_milieu_base` associates the
medium with the momentum equation *only* (the base `Probleme_base` version
associates with every equation). Both coupled subclasses therefore override it
as `Pb_Hydraulique*(mil); eq_magnetic_field.associer_milieu_base(mil);`.

**`nb_composantes` stays a problem-level head keyword** (read before the base
`readOn`), because `Convection_Diffusion_Magnetic_field::discretiser()` needs it
before the unknown is built.

**The k-eps model assumes equation 1 is a scalar transport equation.**
`Modele_turbulence_hyd_K_Eps::preparer_calcul()` (and its `_Bicephale` twin,
in `../triocfd-code`) initialised the thermal wall law with
`equation(1).get_modele(TURBULENCE).valeur()` whenever
`nombre_d_equations() > 1`. In `Pb_MHD_Turbulent` equation 1 is the magnetic
equation, which has no turbulence model, so the null reference aborted
(`TRUST_Ref.h:134: assertion 'p_ != nullptr' failed`). Both call sites now test
the reference before dereferencing it. This is the same ordering constraint as
below: the thermal equation, when it lands, goes **before** the magnetic one.

**Strong coupling** (`couplage_fort`) lives in one free function,
`mhd_iterate_time_step()` (`src/MHD_Coupling.{h,cpp}`), shared by both coupled
classes; it calls `pb.Probleme_base::iterateTimeStep()` explicitly to avoid
re-entering the override.

## 21. Two `Op_Diff_PolyMAC_MPFA` out-of-bounds reads with a vector unknown (2026-10-22)

Found while validating the hierarchy above under `semi_optim` (asserts on).
Both are **pre-existing** and were invisible in the `optim` build, where
`NDEBUG` compiles the bounds check away and the stray read happens to hit the
next element of a uniform field (so the results were right by luck):

1. `Op_Diff_PolyMAC_MPFA_base::update_nu()` (and its `HFV` twin):
   `nu_src(!c_nu * e, n)` with `n < N` (unknown components) while the
   diffusivity line has `N_mil` (= `masse_volumique().line_size()`) entries.
   With a scalar medium (`rho` = 1 component) and a vector unknown
   (`nb_composantes = 2`) that reads past the end of the line.
2. `Op_Diff_PolyMAC_MPFA_Elem::calculer_dt_stab()`:
   `lambda(!cL * e, n)` / `diffu(!cD * e, n)` with the same mismatch.

Both are guarded upstream in `../trust-code` (outside this repo's diff):
a one-component source field is now broadcast to every component
(`nu_src_scalaire`, `nl` / `nd`). Numerically identical when the field is
uniform; defined behaviour otherwise.

Symptom when unguarded: `TRUSTTab.tpp:71: Assertion 'i2 >= 0 && i2 <
dimensions_[1]' failed` from `Op_Diff_PolyMAC_MPFA_Elem::calculer_dt_stab()`
during `Schema_Temps_base::initialize()`, i.e. right after "Pressure increment
computed successfully".

---

## 22. Consolidation review of the coupled (low-Rm) path (2026-10-24)

Review checklist requested by `PLAN_mhd_coupled.md` §4.4, applied to the
low-Rm two-way path (`Pb_MHD`, `Pb_MHD_Turbulent`, `Terme_Source_Lorentz`,
`MHD_Coupling`, `vitesse_hydraulique` / `induction_stretching`).

### Sign conventions — resolved, validated

- Lorentz force is \f$ \mathbf{F} = \mathbf{J}\times\mathbf{B} \f$ with
  \f$ \mathbf{J} = \operatorname{curl}\mathbf{b}/\mu_0 \f$, assembled as
  `mhd_force_lorentz` (`src/MHD_posttraitement_kernel.h`):
  2D \f$ (F_x,F_y)=(-w_z B_y,\; w_z B_x)/\mu_0 \f$,
  3D \f$ \mathbf{F}=(\mathbf{w}\times\mathbf{b})/\mu_0 \f$.
  The force is divided by \f$\rho\f$ (per unit mass) in `ajouter_blocs`,
  matching the gravity/Boussinesq precedent.
- The sign is pinned by validation, not by convention alone:
  `ForceLorentzBalance` (hydrostatic \f$\Delta p = -0.18\f$, \f$u\to 0\f$),
  `MMS_Coupled` (both force components, \f$\Delta p=-0.60\pm5\%\f$),
  `HartmannTwoWay` (suppression by \f$12.7\times\f$ at Ha=5) and
  `Coupled_Feedback` (two-way discriminator). A sign flip in either the
  Lorentz force or the stretching source fails these at the 0.4–1 % level.
- Induction sign follows the kernel convection–diffusion convention
  \f$ \partial_t b + (u\cdot\nabla)b - \eta\Delta b = (b\cdot\nabla)u \f$;
  `induction_stretching` contributes \f$ +(b\cdot\nabla)u \f$ to the RHS.

### Boundary conditions — resolved, validated

- Magnetic CLs reuse the kernel scalar CLs through the
  `domaine_application() == "Thermique"` contract
  (`paroi_isolante_magnetique` = homogeneous Neumann on every component,
  `Neumann_Paroi_adiabatique`, `Paroi`, `frontiere_ouverte_pression_imposee`).
  The kernel validates the CL vocabulary; the module only supplies the
  `Thermique` application tag.
- Coupled open-pressure boundaries must impose the analytic magnetic-pressure
  profile \f$ p(y)=P-b_x(y)^2/(2\mu_0) \f$; a uniform \f$p\f$ drives a
  spurious cross-flow \f$u_y\sim5\times10^{-4}\f$ and pollutes \f$b_y\f$
  (measured drift 9.6 % → 0.75 %). `HartmannTwoWay` checks
  \f$\max |u_y|/U_0 < 10^{-3}\f$ at every Re/Ha point.
- Insulating walls are homogeneous Neumann on all components; the
  `∇b` reconstruction at a wall cell uses the zero-normal-gradient trace
  \f$ b_f = b(e_0) \f$, which is O(h) accurate there (see §11bis).
- `force_lorentz` skips Dirichlet-velocity faces (`fcl(f,0) >= 2`) in the
  face block and carries the force through the element block, exactly as the
  kernel `Terme_Boussinesq_PolyMAC_CDO_Face` source does.

### Matrix properties — resolved (P5), documented in §12bis

- `Terme_Source_Lorentz::ajouter_blocs` modifies only `secmem` (RHS); it
  never touches the Jacobian. The Lorentz force is explicit and lagged
  (\f$\mathbf{J}\times\mathbf{B}\f$ at \f$b^s\f$), so it does not enter
  the momentum matrix at all.
- Matrix symmetry is therefore a kernel-level property: the PolyMAC_MPFA
  diffusion operator is symmetric, the upwind convection operator is not.
  No module-level symmetry break is introduced by the coupling.
- The VEF branch assembles the force per element and distributes it through
  `volumes_entrelaces`; no matrix contribution there either.
- If a future implementation treats the Lorentz force implicitly (in the
  matrix), symmetry must be revisited.

### Wall-gradient bias — remains a documented limitation

The M1 `induction_stretching` source reads \f$\nabla u\f$ from the kernel's
face-gradient average; a boundary face only gets a Dirichlet contribution, so
the two cells adjacent to a wall receive a biased gradient (measured exactly
halved for a linear velocity). `Hartmann1D` documents and bounds the effect;
the regression bound is **not** tightened. The fix lives in the kernel
(`grad_Champ_Face_PolyMAC_MPFA::update_ge()`), not in this module, and is
tracked as an upstream item. Core probes are asserted tightly; wall probes
only against a bound.

### Sanitizer status (open)

The module-only ASan/UBSan environment is not configured in this checkout
(no `$TRUST_ROOT/env/Cmake.custom`, no `linux_custom` symlinks, no
`TRUST_mpi_custom.a`). The recipe in §18.2 of this file is complete and
was last exercised on 2026-10-20 (module-instrumented build), where it
localised the §18.2 kernel FPE. A full-kernel instrumented pass
(`make custom` in the TRUST checkout) is still required to instrument
kernel-side reads; until then, the coupled paths are validated by the
analytic notebooks (ForceLorentzBalance, MMS_Coupled, HartmannTwoWay,
Coupled_Feedback) and the 29-test arith-only unit suite, not by ASan.
