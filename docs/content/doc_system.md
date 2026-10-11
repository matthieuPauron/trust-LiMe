@page TrustLiMe_docsys Documentation system

This page explains **how this documentation is built and how to extend it**. It
contextualises the TRUST platform's documentation machinery and describes how
TRUST-LiMe reuses it.

---

## 1. One pipeline, shared with the whole TRUST platform

TRUST-LiMe is a *baltik* module, and TRUST ships a documentation pipeline
designed to be **shared** between the kernel and every baltik. The same HTML
build, the same theme, the same keyword-reference generator, and the same
post-processing scripts are used for all of them.

The important property is that the documentation pipeline is **fully orthogonal
to the compilation pipeline**:

- building the docs does **not** require running `./configure`, compiling the
  module, or having a working `trust-lime` executable;
- building the module never triggers the documentation build.

The only requirement is that `TRUST_ROOT` points at a TRUST checkout containing
its `docs/` tree.

### The pieces

| Piece | Role |
|---|---|
| [Doxygen](https://www.doxygen.nl/) | Reads the C++ sources and the Markdown pages and renders the HTML site. |
| [doxygen-awesome](https://github.com/jothepro/doxygen-awesome-css) | The visual theme (sidebar layout, dark-mode toggle). |
| [trustify](https://github.com/cea-trust-platform/trustify) | Extracts the `// XD` keyword tags from the C++ sources and generates the **keyword reference manual**. |
| [MathJax 3](https://www.mathjax.org/) | Renders the LaTeX formulas; a local copy is bundled into the output so the site works offline. |

### Where the shared rules live

TRUST-LiMe's `docs/` directory is deliberately tiny. Most of the logic lives in
the TRUST checkout and is `include`d at build time:

| File | Owner | Provides |
|---|---|---|
| `docs/Makefile` | **TRUST-LiMe** | Identity (`PROJECT_NAME`, paths) and the two-phase build. Includes the two files below. |
| `$TRUST_ROOT/docs/common.mk` | TRUST | Doxygen/MathJax resolution and installation, trustify installation, the `run_doxygen` recipe and its post-processing. |
| `$TRUST_ROOT/docs/baltik.mk` | TRUST | The baltik-specific `Doxyfile` bootstrap and the `doxyfile_chain` (dependency `INPUT` extension). |
| `$TRUST_ROOT/docs/doxyfile_common` | TRUST | The shared Doxygen configuration (theme, extraction, search, MathJax, graphs). |

Because the shared files are re-read on **every** `make`, upstream improvements
(theme updates, a new Doxygen version, new post-processors) reach TRUST-LiMe on
the next build with no per-project patch.

### What TRUST-LiMe owns

| File | Regenerated? | Notes |
|---|---|---|
| `docs/Makefile` | once (scaffolded) | Thin shim — safe to edit and commit. |
| `docs/Doxyfile` | once, on the first `make` | Generated from the TRUST baltik template, then left alone — safe to edit and commit. |
| `docs/theme/header.html` | once (scaffolded) | The HTML page chrome (GitHub corner, fonts, dark-mode init). Safe to edit. |
| `docs/content/main_page.md` | once (scaffolded) | The `@mainpage` home page. |
| `docs/content/*.md` | authored | The narrative pages (this one included). |

Everything the build produces lands under `docs/build/`, which is gitignored and
wiped by `make clean`. **Never edit anything under `docs/build/`.**

---

## 2. Building the documentation

From the repository root:

```bash
make docs          # convenience wrapper -> make -C docs
```

or directly:

```bash
cd docs && make
```

Both run two phases:

1. **content generation** — `trustify generate_markdown` writes the keyword
   reference under `docs/build/generated_content/`;
2. **the Doxygen run** — Doxygen scans the sources and pages, then two Python
   post-processors flatten the navigation tree and customise the search bar.

The result is `docs/build/html/index.html` — a self-contained, browser-readable
site. For a **fast HTML-only rebuild** after editing a narrative page, skip
regeneration and just re-run Doxygen:

```bash
cd docs && make run_doxygen
```

`make help` (from `docs/`) lists every target.

### Prerequisites and environment

- `TRUST_ROOT` must be set. Sourcing `env_trust_LiMe.sh` (which sources
  `env_TRUST.sh`) does this for you.
- **Doxygen** is not installed system-wide: `common.mk` unpacks the version
  pinned by TRUST from `$TRUST_ROOT/externalpackages/`, or downloads it from
  `doxygen.nl` when that tarball is absent. To use a native build instead:

  ```bash
  TRUST_DOXYGEN_BINARY=$(command -v doxygen) make
  ```

- **trustify** is run with the interpreter TRUST resolves for its own tooling.
  On this machine that is the conda environment at
  `$TRUST_ROOT/exec/python`, which already ships `trustify` and `pydantic`.
- `graphviz` (`dot`) is optional and only needed for class/call graphs.

---

## 3. Writing a page

Narrative pages live in `docs/content/` (Doxygen scans it recursively, so a new
`.md` file needs no Doxyfile edit). The rules are the standard TRUST ones:

1. **Declare the page** with a unique id on its first line:

   ```
   @page TrustLiMe_myTopic My topic title
   ```

   Use a `CamelCase` id prefixed with `TrustLiMe_` to avoid clashing with TRUST's
   own pages.

2. **Wire it into the navigation tree** from `main_page.md` (or from another
   page) with `@subpage`:

   ```
   - @subpage TrustLiMe_myTopic
   ```

   A page that is not referenced by another page is built but unreachable —
   an *orphan*.

3. **Headings** start at `##` (the `@page` directive already provides the H1).
   On `main_page.md`, avoid Markdown `##` headings next to `@subpage` links —
   Doxygen would nest them under the nearest subpage. Use a raw `<h2>` tag
   there instead, as this project's home page does.

4. **Math** uses Doxygen's delimiters, *not* `$…$` (which would clash with code
   samples):

   - inline: \f$ \eta = 1/(\mu_0\sigma) \f$ → `\f$\eta = 1/(\mu_0\sigma)\f$`
   - display: `\f[ … \f]` on their own lines
   - multi-line: use a LaTeX `aligned` environment inside `\f[ … \f]`

5. **Cross-references**: `@ref TrustLiMe_model "the model page"` links to another
   page; `@ref` also auto-links C++ identifiers written in backticks.

6. **Callouts**: `@note`, `@warning` and `@attention` render as styled boxes.
   Avoid `@todo`: the shared configuration disables the todo list
   (`GENERATE_TODOLIST = NO` in `doxyfile_common`), so its content is silently
   dropped — write such markers as prose instead.

7. **Figures**: place images in a directory (e.g. `docs/images/`), add that
   directory to `IMAGE_PATH` in `docs/Doxyfile`'s user-override block, and
   reference the image by its bare file name.

### Checklist for a new page

- [ ] The file opens with `@page TrustLiMe_<id> <Title>`.
- [ ] The id is referenced with `@subpage` from a parent page.
- [ ] Math uses `\f$…\f$` / `\f[…] \f`, never `$…$`.
- [ ] `cd docs && make` completes without warnings about undefined references.
- [ ] Both light and dark mode look correct in a browser.

---

## 4. What appears in the sidebar

The documentation has three kinds of content, and Doxygen assembles them into one
navigation tree:

- **Narrative pages** — the hand-written Markdown under `docs/content/`, rooted
  at the `@mainpage` home page (this is where the pages you are reading live).
- **C++ API** — classes, namespaces and functions extracted from the `/** */`
  doc blocks in the TRUST kernel sources and in this module's `src/`. TRUST's
  `docs/theme/DoxygenLayout.xml` controls the sidebar tabs.
- **Keyword reference** — generated by trustify from the `// XD` tags in the
  sources. It is linked from the home page as `@subpage KeywordsReference`; the
  page id must match the one trustify emits.
