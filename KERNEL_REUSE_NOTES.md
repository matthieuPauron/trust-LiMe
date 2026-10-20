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

### Path 1 — Lightweight Green-Gauss correction (current implementation)
- Uses the existing gradient_b_elements — no new discretisation infrastructure
- **Exact for linear fields at interior cells** (all faces have neighbours)
- Boundary cells have O(h) accuracy due to the "zero normal gradient" approximation
  (bf = b(e0) at walls); this is the same approximation pattern as the
  thermalhydraulics velocity field (∇·u = 0 assumed, not enforced)
- **Validation probes must target interior cells**
- All 22 MagneticStandalone validation cases pass with this treatment
- Documented in AGENTS.md §Divergence of the magnetic field` and
  KERNEL_REUSE_NOTES.md §11bis

### Path 2 — Pressure-correction approach (alternative, beyond M0/M1 scope)
- Would achieve **machine-precision ∇·b = 0 globally** (to solver tolerance)
- **Requires new solver infrastructure**: a PISO-like algorithm for the magnetic field,
  similar to the thermalhydraulics Navier-Stokes fluide_incompressible block
- **Beyond M0/M1 standalone scope**: would be Phase 1+ of the coupled MHD plan
  (PLAN_mhd_coupled.md)
- **Implementation steps** (beyond current scope):
  1. Add divergence_free { } keyword to .data file
  2. Add terme_divergence_free term to the equation
  3. Solve a Poisson-type correction equation ∇·((1/μ₀)∇p_mag) = ∇·b
  5. Correct the field: b_corrected = b − μ₀∇p_mag
  5. Update nombre_d_equations() if adding coupled equations
  6. Revalidate all 22 validation cases
  7. Document in model.md, test_cases.md, AGENTS.md, KERNEL_REUSE_NOTES.md
- **Acceptance criteria** (future work):
  - ∇·b machine-precision globally
  - All existing validation cases still pass
  - Sanitizer pass (ASan/UBSan)
  - Review checklist signed off

**Note**: Path 2 is documented for completeness and future reference; it is
not implemented in the current M0/M1 standalone scope. The baseline treatment
(Path 1) is what is used in all existing validation cases and is consistent
with the thermalhydraulics velocity-field assumption pattern.

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
(`src/Terme_Source_Lorentz.{h,cpp}` + optional NS equation in `Pb_MHD`):

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
mesh-convergence/notebook-merging session. The geometry: Pb_MHD holds the
magnetic equation as equation 0; the optional `Navier_Stokes_standard` is
created, discretised and read *after* the magnetic block
(`Probleme_base::read_optional_equations`).

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
  token after `x` is `=`, the value is at +2). `NombreDeReynoldsMagnetique`
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

### 18.2 Layout-dependent SIGFPE in the parallel path (CourantMagnetique fine — open)

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
scale of this one form runs on 1 processor; the sequential probe values
reproduce the historical L2 values exactly (the RULE compare for the form
is green). **The underlying memory bug must be pinned by the sanitizer pass
(ASan/UBSan, `make module_custom`)** — until then any layout shift
(name, path, build) can flip the crash on or off for any form.


### 18.3 The coupled error path segfaults after printing its diagnostic (open)

All four coupled malformed-input datasets print their diagnostic and then
die with **SIGSEGV (`rc=139`)** instead of a clean non-zero exit
(`rc=255`, like the standalone error forms). Pre-existing since the forms
were created (the historical notebooks show the same `rc=139` tails — the
bash wrapper's crash report, which embeds the volatile PID, is what lands
in the `tail[0][:90]` print line); the merged `ErrorInputs` form keeps the
behaviour and its asserts treat any non-zero exit as the pass condition.
Same family suspicion as §18.2 (the diagnostic goes through
`Process::exit`-adjacent cleanup paths) — fold into the sanitizer pass.
