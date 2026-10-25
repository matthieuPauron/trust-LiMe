@page TrustLiMe_quickstart Quick Start

This page gets you from a fresh checkout to a running TRUST-LiMe build and a
generated copy of this documentation.

## Prerequisites

- A compiled **TRUST** kernel in a sibling checkout (`../trust-code`).
- The TRUST Python environment (`venv_trust`).
- CMake / the baltik toolchain provided by TRUST.

See the project `docs/content/readme.md` and `AGENTS.md` for host-specific notes (laptop vs.
workstation, thread counts).

## Sourcing the environment

Every terminal that uses TRUST-LiMe must first load the environment:

```bash
source env_trust_LiMe.sh
```

This exports `TRUST_ROOT`, `project_directory`, `exec_opt`, and activates the
TRUST Python virtualenv. Equivalently:

```bash
bash .opencode/scripts/with-trust-env.sh <command>
```

## Building the module

From the repository root:

```bash
make module_semi_optim   # module only, -O3, asserts on (recommended default)
make module_optim        # optimized
make module_debug        # debug
make module_custom       # uses project.cfg cpp_flags/ld_flags (sanitizers)
```

Full (kernel + module) builds are `make optim` / `make semi_optim` / `make debug`
and are slow; prefer `module_*` targets.

## Syntax check (fast)

```bash
bash .opencode/scripts/syntax-check.sh src/Pb_MHD.cpp
```

## Generating this documentation

Independent of the build:

```bash
make docs
# or directly:
cd docs && make
```

HTML output is written to `docs/build/html/index.html`.

## Running the tests

```bash
.opencode/scripts/unit-tests.sh --arith-only   # micro + numeric kernels (seconds)
.opencode/scripts/unit-tests.sh                # full suite (needs full TRUST build)
```

## Validation cases

Notebooks live under `share/Validation/Rapports_automatiques/`. Run them with
`/validate` after `make check-validation`; see `trust-lime-notebooks`.

## Where to go next

- @ref TrustLiMe_intro "Introduction" — what the module does.
- @ref TrustLiMe_model "Problems, equations and boundary conditions".
- @ref TrustLiMe_testcases "Test cases" and @ref TrustLiMe_validation "Validation cases".
- @ref TrustLiMe_docsys "Documentation system" — how these pages are authored.