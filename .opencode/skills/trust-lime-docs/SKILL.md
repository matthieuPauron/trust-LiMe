---
name: trust-LiMe documentation
description: >
  Build, extend, and publish the trust-LiMe web documentation. TRUST-LiMe is a
  baltik module, so its docs reuse TRUST's shared Doxygen + doxygen-awesome +
  trustify pipeline: narrative Markdown pages under docs/content/, the C++
  kernel API, and the auto-generated keyword reference, rendered to
  docs/build/html/. Use when adding or editing documentation pages, running
  `make docs`, debugging the doc build, or opening the site in a browser.
---

# trust-LiMe documentation

TRUST-LiMe documents itself the way every TRUST **baltik** does: a small
`docs/` tree in this repo that reuses the TRUST platform's shared documentation
pipeline. The output is a static HTML site readable in any browser. It is
**decoupled from the compilation build** — you do not need to compile the module
to write or build the docs.

## When to use this skill

- Adding, editing or reorganising documentation pages.
- Running or debugging `make docs` / `cd docs && make`.
- Opening the rendered site in a browser.
- Explaining how the trust-LiMe docs relate to the TRUST docs.

## Big picture

| Piece | Where it lives | Role |
|---|---|---|
| `docs/Makefile` | **this repo** | Identity (`PROJECT_NAME = trust_LiMe`) + the two-phase build; `include`s the TRUST shared rules. |
| `docs/Doxyfile` | **this repo** | Generated once from the TRUST baltik template; safe to edit/commit. |
| `docs/theme/header.html` | **this repo** | Page chrome + GitHub corner + dark-mode init. |
| `docs/content/*.md` | **this repo** | The narrative pages (the ones you edit). |
| `$TRUST_ROOT/docs/common.mk` | TRUST | Doxygen/MathJax install, trustify install, the `run_doxygen` recipe + post-processing. |
| `$TRUST_ROOT/docs/baltik.mk` | TRUST | Baltik `Doxyfile` bootstrap + `Doxyfile.chain` (ancestor `INPUT`). |
| `$TRUST_ROOT/docs/doxyfile_common` | TRUST | Shared Doxygen config (theme, MathJax, search, graphs). |
| `$TRUST_ROOT/bin/baltik/templates/basic/docs/` | TRUST | The scaffold copied by `trust -baltik_docs_scaffold`. |

Everything generated lands under `docs/build/` (gitignored, wiped by
`make clean`). **Never edit anything under `docs/build/`.**

## Build

Always source the environment first: the docs Makefile requires `TRUST_ROOT`.

```bash
source env_trust_LiMe.sh          # sets TRUST_ROOT (via env_TRUST.sh)

# Full build: trustify keyword reference + Doxyfile.chain + Doxygen + post-processing
cd docs && make                   # or, from the repo root: make docs

# Fast HTML-only rebuild (editing narrative pages): skip trustify
cd docs && make run_doxygen
```

Output: `docs/build/html/index.html`.

`make help` (from `docs/`) lists every target; `make clean` wipes `docs/build/`.

### Toolchain (already resolved on this machine — do not pre-install)

- **Doxygen** is not installed system-wide. `common.mk` unpacks the pinned
  version from `$TRUST_ROOT/externalpackages/`, or downloads it from
  `doxygen.nl` on first use (Doxygen 1.16.1, ~100 MB). To force a native build:
  `TRUST_DOXYGEN_BINARY=$(command -v doxygen) make`.
- **trustify** is run through the interpreter TRUST resolves for its own
  tooling — here the conda env at `$TRUST_ROOT/exec/python`, which already
  ships `trustify` and `pydantic`. No `pip install` is needed; if `trustify` is
  ever missing, `make install_trustify` puts it there.
- **graphviz** (`dot`) is optional, only for class/call graphs.
- **MathJax 3** is bundled into the HTML output, so the site works offline.

### Recreating `docs/` from scratch

If `docs/` is ever missing, scaffold it from the repo root:

```bash
TRUST_ROOT=/path/to/trust-code bash $TRUST_ROOT/bin/trust -baltik_docs_scaffold
```

This writes `docs/{Makefile,content/main_page.md,content/demo.md,theme/header.html,.gitignore}`.
The `Doxyfile` is regenerated on the first `make`. Then delete the placeholder
`docs/content/demo.md` and add real pages.

## View it in a browser

```bash
chromium "file://$PWD/docs/build/html/index.html"
# or serve over HTTP (avoids any file:// storage quirks):
cd docs/build/html && python3 -m http.server 8000    # -> http://localhost:8000/
```

## Add or edit a page

Narrative pages live in `docs/content/` (Doxygen scans it recursively, so a new
`.md` file needs no Doxyfile change). Follow the TRUST conventions:

1. **Declare the page** with a unique id on its first line. Prefix ids with
   `TrustLiMe_` to avoid clashing with TRUST's own pages:

   ```
   @page TrustLiMe_myTopic My topic title
   ```

2. **Wire it into the navigation tree** from `main_page.md` (or another page)
   with `@subpage`; an unreferenced page is an orphan:

   ```
   - @subpage TrustLiMe_myTopic
   ```

3. **Headings start at `##`** (the `@page` directive is the H1). On
   `main_page.md`, do **not** put Markdown `##` headings next to `@subpage`
   links — Doxygen would nest them under the nearest subpage. Use raw `<h2>`.

4. **Math** uses Doxygen delimiters, never `$…$` (which clashes with shell
   snippets):
   - inline: `\f$ \eta = 1/(\mu_0\sigma) \f$`
   - display: `\f[ … \f]` on their own lines
   - multi-line: a LaTeX `aligned` environment inside `\f[ … \f]`

5. **Cross-references:** `@ref TrustLiMe_model "the model page"`.
   `@ref` also auto-links C++ identifiers written in backticks.

6. **Callouts:** `@note`, `@warning`, `@attention` render as styled boxes.
   **Avoid `@todo`** — the shared config sets `GENERATE_TODOLIST = NO`, so its
   content is silently dropped. Also never write `@` commands inside
   `*emphasis*`: `*@todo …*` swallows the closing `*` and breaks the parser.

7. **Figures:** put images in e.g. `docs/images/`, add that directory to
   `IMAGE_PATH` in `docs/Doxyfile`'s user-override block, and reference the
   image by its bare file name.

## Verify

After a build, check for warnings that come from **your** pages:

```bash
grep docs/content docs/build/doxygen_warnings.log || echo "our content is clean"
```

`docs/build/doxygen_warnings.log` has ~350k lines from the TRUST kernel sources
and the trustify-generated keyword pages — those are upstream noise. Any warning
mentioning `docs/content/` is yours to fix. Confirm the site in a browser in
both light and dark mode.

## Files currently in the tree

- `docs/content/main_page.md` — `@mainpage`: overview, navigation, quick start, credits.
- `docs/content/intro.md` — `TrustLiMe_intro` Introduction.
- `docs/content/model.md` — `TrustLiMe_model` problems, equations, boundary conditions.
- `docs/content/test_cases.md` — `TrustLiMe_testcases` unit + validation datasets.
- `docs/content/validation.md` — `TrustLiMe_validation` Jupyter validation cases.
- `docs/content/doc_system.md` — `TrustLiMe_docsys` this pipeline, in depth.

## Pitfalls

- **`.gitignore`**: the baltik `.gitignore` ignores every `Makefile`; the repo
  adds `!docs/Makefile` so the authored doc Makefile stays committable. Keep
  that negation. `docs/build/` is ignored by `docs/.gitignore`.
- **Run `make` from inside `docs/`** (or via the root `make docs`): several
  paths resolve relative to `CURDIR`.
- **`TRUST_ROOT` must be set** or the docs Makefile errors immediately.
- The keyword reference page id is **`KeywordsReference`**; the `@subpage`
  reference in `main_page.md` must match what trustify emits.

## Reference (upstream)

- `$TRUST_ROOT/docs/README.md` — shared doc build system.
- `$TRUST_ROOT/docs/How_to_write_doc.md` — authoring guide.
- `$TRUST_ROOT/bin/baltik/README_INSTALL.md` — baltik docs pipeline.
