---
name: trust-LiMe coding
description: >
  Write and modify trust-LiMe C++ kernel-extension code: deriving baltik
  classes from TRUST kernel bases, registering dataset keywords, reading
  parameters, association/ref patterns, TRUST coding style rules, strict
  compiler flags, and trustify // XD keyword documentation. Use when
  editing src/*.{h,cpp}, adding a class or keyword, or fixing compile
  warnings/errors.
---

# trust-LiMe coding (C++ baltik module)

## First: read docs/content/dev_corner/kernel_reuse.md

`docs/content/dev_corner/kernel_reuse.md` (repo root) is the accumulated knowledge base:
kernel class choices, keyword-reading order, CL compatibility rules, the
`Champ_Inc_base` transporting-velocity requirement, field factories, and
pointers to exact kernel sources. **Read it before extending the module;
append new findings to it when done.**

The file is long (1700+ lines). Prefer targeted retrieval over a full read:

```bash
.opencode/scripts/retrieve-notes.sh <topic words>   # best-matching sections
.opencode/scripts/retrieve-notes.sh --list          # browse headings
```

Kernel sources are the `trust-kernel` / `trust-include` references
(`../trust-code/src/Kernel`, `../trust-code/include`).

## Canonical recipe for a new keyword/problem (official DevTutorials)

1. **Design the data file first.** Write the desired `.data` syntax;
   keywords map 1:1 to C++ classes. This identifies what to implement.
2. **Maximize kernel reuse.** Inspect the Doxygen inheritance graphs of
   `Probleme_base` / `Equation_base` and derive from the closest class
   (e.g. `Convection_Diffusion_Temperature`) instead of reimplementing.
3. Derive and override the virtual hooks: `set_param`,
   `lire_motcle_non_standard`, `discretiser`, `preparer_calcul`,
   `assembler`, `assembler_blocs`, `associer_milieu_base`,
   `nombre_d_equations()`, `equation(int)` (for problems).
4. Register the class: `Declare_instanciable` in the header,
   `Implemente_instanciable(Class, "keyword", Parent)` in the `.cpp` —
   the string is the dataset keyword.
5. Verify with `syntax-check.sh`, then `make module_semi_optim`, then
   the `trust-lime-testing` skill's gate.

## Idioms

- **Associations with `REF(...)`** instead of raw pointers (deliberate
  anti-memory-leak technique):
  `REF(Constituant_Avec_Vitesse)` + `#include <TRUST_Ref.h>`.
- **Reading** with `Param`:
  ```cpp
  Param param(que_suis_je());
  param.ajouter("option", &opt_);
  param.ajouter("eta", &eta_, Param::REQUIRED);
  param.lire_avec_accolades_depuis(is);
  ```
  Non-standard keywords go through `lire_motcle_non_standard`; unknown
  tokens bubble up to `Interprete` and abort naming the token.
- **Output**: `Cout` (physics, master proc) and `Cerr` (warnings/errors),
  terminated by `finl`; fatal errors end with `Process::exit();`.
- Kernel headers are included **with** the `.h` extension:
  `#include <Convection_Diffusion_Temperature.h>`.
- File output names: build a `Nom` from `Objet_U::nom_du_cas()` and write
  via `SFichier`; per-proc plumbing via `Process::Journal()`.

## Style rules (warnings are errors)

- **C++20**, `-mavx`, `-O3`, `-Werror` with `-Wall -Wextra -Wconversion
  -Wshadow -Woverloaded-virtual -Wnon-virtual-dtor -pedantic`. **Keep
  warnings at zero.** `-Wconversion` is the usual trap (implicit
  `double`→`int`, sign mismatch) — cast explicitly.
- Class data members suffixed with `_`; static methods start upper-case.
- No unnecessary braces; `for`/`if` bodies on separate lines.
- **Doxygen block mandatory at the head of every method**:
  `/*! @brief ... @param[in] ... @param[out] ... @return ... */`,
  `@see` for related methods.
- TRUST I/O: `Sortie`/`Entree` with `printOn`/`readOn`.
- French method names and keyword names (TRUST convention).
- Containers: `DoubleTab`/`DoubleVect`; parallel reductions via
  `mp_sum()` (we still target sequential runs; see AGENTS.md).
- A `.clang-format` exists but auto-format on write is **off**; enable
  deliberately.

## Fast loop

After every meaningful edit: `bash .opencode/scripts/syntax-check.sh
src/<file>.cpp` (g++ `-fsyntax-only`, real flags from
`compile_commands.json`, forces `-std=c++20`). Only then
`make module_semi_optim`.

## Committing rules (per parallel session / per item)

- **One commit per parallel session and per item** — each independent workstream (parallel session / Hermes Agent profile) creates its own atomic commit; do not squash or merge commits from different sessions into a single change.
- **Documentation may be committed separately** — `.md` and other documentation files can be committed in a distinct commit from code changes, which simplifies the committing job and makes review bisectable.
- **Validation files are part of the codebase** — `.ipynb` validation files must be committed alongside the associated codebase change (the physics or implementation they validate) to keep the repository state consistent and verified.

These rules apply on top of the existing fast‑loop checks (syntax-check → module_semi_optim).

## trustify `// XD` keyword documentation

Annotate readable classes so the keyword reference / dataset validation
works (`make docs` consumes these):

- Tags go in **`.cpp` (or `.xd`) only — tags in `.h` are silently
  ignored**.
- Header line right after the `Implemente_instanciable`:
  `// XD <name> <parent_kw> <name_or_syno> <BRACE|NO_BRACE|INHERITS_BRACE> <desc>`
- Inline: `param.ajouter("tutu",&tutu_); // XD_ADD_P floattant desc`
  (required inferred from `Param::REQUIRED`).
- Standalone: `// XD attr <name> <type> <syno> <REQ|OPT> <desc>`;
  continuation `// XD CONT`; types: `entier`, `floattant`, `chaine`,
  `rien/flag`, lists, `chaine(into=[...])`, `entier(min=,max=)`.
- Validate: `trustify check <file.data>`.

## Reference (upstream)

- https://cea-trust-platform.readthedocs.io/en/v1.9.8/DevTutorials.html
  (Build your Baltik; BALTIK convection–diffusion tutorial)
- https://cea-trust-platform.readthedocs.io/en/v1.9.8/Dev_CodingGuidelines.html

## Porting a PolyMAC `Champ_Fonc` to VEF

When a `Champ_Fonc_*` derives from `Champ_Fonc_Elem_PolyMAC_CDO` and needs to
run on a VEF mesh, add a parallel `*_VEF` class rather than generalising the
original:

1. Derive from `Champ_Fonc_P0_VEF` (include `<Champ_Fonc_P0_VEF.h>`); keep the
   same `configurer`/`calculer`/`mettre_a_jour` logic. In `configurer`, call
   `associer_domaine_dis_base(eqn.domaine_dis())` and size values with
   `fixer_nb_valeurs_nodales(le_dom_VF.valeur().nb_elem())`.
2. In `mettre_a_jour`, forward to `Champ_Fonc_P0_VEF::mettre_a_jour(tps)` (not
   the PolyMAC base). In `calculer`, use only `valeurs()`, operand `valeurs()`,
   and `Objet_U::dimension` — nothing PolyMAC-specific.
3. Register with `Implemente_instanciable(..., "Champ_Fonc_*_VEF", Champ_Fonc_P0_VEF)`.
4. The new `.cpp`/`.h` are auto-discovered by the baltik `file(GLOB ...)` in
   `build/src/CMakeLists.txt`; re-run `cmake .` in `build/src/exec_semi_opt` (or
   `make module_semi_optim`) to compile. Verify first with `syntax-check.sh`.
5. Equation-side wiring (instantiating the VEF variant when the discretisation
   is VEF) is a separate step; the classes alone do not switch behaviour.

## Project-local references

- `references/trust-project-access.md` — efficient access to the TRUST project: syntax‑check loop, module‑only builds, targeted unit‑test suffixes, and symbol lookup without full‑tree indexing.


## Hermes usage

In Hermes there are no OpenCode slash commands. Run the scripts above via the
`terminal` tool (paths are repo-relative), read/edit files with `read_file` /
`patch` / `write_file`, use `execute_code` for bulk checks, and `delegate_task`
for parallel analysis. Heavy builds must pass `.opencode/scripts/check-heavy-build.sh`
and follow the RAM guard in `AGENTS.md`.
