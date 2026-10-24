---
name: trust-LiMe testing
description: >
Run and extend trust-LiMe tests: GoogleTest unit tests in tests/UnitTests/
(via .opencode/scripts/unit-tests.sh), baltik non-regression suites
(make check_*/ctest_*), and dataset-level validation cases. Use when
writing or running any test, triaging failures, or adding a new .data
case to a validation notebook. The green-baseline gate and the ASN 28
staging sequence live here.
---

# trust-LiMe testing

Two test tiers, from fast to slow. Always climb from the cheapest tier that
can detect the failure.

| Tier | What | Command | Needs |
|---|---|---|---|
| 0 | Syntax check (one file) | `.opencode/scripts/syntax-check.sh src/X.cpp` | nothing — seconds |
| 1 | Unit tests (GoogleTest) | `.opencode/scripts/unit-tests.sh [--arith-only]` | arith-only: micro/numeric kernels only; full: kernel built |
| 2 | Module build | `make module_semi_optim` | kernel built |
| 3 | baltik non-regression | `make check_optim` / `make ctest_optim` | full build; refs in `tests/Reference/` |
| 4 | Notebook / dataset validation | see `trust-lime-notebooks` skill | full TRUST build + venv |

## Tier 1 — Unit tests

Follow the kernel convention (`../trust-code/tests/UnitTests/`):
`TEST(Suite, Name)`, `EXPECT_*`, `EXPECT_DEATH`, inline `EChaineJDD`
parsing — no mesh, no solver.

```bash
.opencode/scripts/unit-tests.sh --arith-only   # fast: micro + numeric kernels
.opencode/scripts/unit-tests.sh                # requires the full TRUST build
```

Baltik generates no `unit` target; the script drives the build itself using
flags from `compile_commands.json` and the link line from baltik's recorded
`link.txt`. See `tests/UnitTests/README.txt` and `KERNEL_REUSE_NOTES.md`
§7ter. Run these before notebook validation — seconds vs minutes.

## Build orchestration

- Prefer `module_*` targets (module only, fast). Full targets (`optim`,
  `semi_optim`, `debug`) rebuild the kernel — slow on both hosts.
- For long builds, run in the background and poll instead of blocking:
  `.opencode/scripts/wait-build.sh [pattern] [poll_seconds]`.
- Build target selection: `/build` (module), `/build-full` (kernel+module),
  `/build-debug` (debug). Pass `$ARGUMENTS` to override the target.
- Environment must be sourced first (see `trust-lime-env` skill) or use
  `.opencode/scripts/with-trust-env.sh`.

## Tier 3 — Non-regression references

References live in `tests/Reference/`. `make check_optim` (baltik) and
`make ctest_optim` (ctest) are also available as `_semi_optim`/`_debug`
variants. `make help` lists all targets.

## Tier 4 — The green-baseline gate (mandatory before adding a case)

A new `.data` case is only trusted if the state before it is green —
otherwise a failure cannot be attributed to the new case alone.

0. **Formatting check.** `.opencode/scripts/check-validation.sh` must be
   green first — it enforces the validation-case formatting rules (R1-R4)
   from the `trust-lime-notebooks` skill (`.ipynb` at the form root only,
   `…/<Category>/<FormName>/<FormName>.ipynb`; no partition/scatter machinery
   in `.data`; one test case per notebook).
1. **Green baseline.** From the repo root: `make module_semi_optim`
   (plus `make semi_optim` if module sources changed), then
   `.opencode/scripts/unit-tests.sh`. Then run **every existing dataset**
   in the target notebook's `src/` and require `exit 0` from each, from a
   clean run directory (delete stale `.stop`, `.out`, `.err`, `*.son`,
   `*.TU` first — a leftover `.stop` halts the solve early). All cases run
   **sequentially** — never pass a processor count.
2. **Add the case.** Copy an existing validated dataset, adapt it, and run
   the *new* case alone — compare its probes against the
   analytic/invariant reference before wiring it into a notebook.
3. **Only then** give the case its own notebook (one test case per
   notebook, R4) and run that notebook.

Applies equally to new unit tests in `tests/UnitTests/`.

## ASN Guide n° 28 staging for new tests

Verification first, then validation against analytical data, and only then
comparison to experiments. A case moves to the next stage once the
previous one is green — never add an experimental-comparison case as the
first test of a new feature. A notebook must run **one and only one test
case** and contain cases from **a single ASN 28 category** (see
`docs/content/test_cases.md` taxonomy; the `Case` folder is the category
grouping).

## Failure triage ladder

1. Compile error → `syntax-check.sh` on the edited file; fix warnings
   (they are errors).
2. Unit test fails → `--arith-only` to isolate micro/numeric kernels.
3. Dataset fails → run it alone from a clean directory; inspect
   `<case>.err`.
4. Don't know the post-processing field name → grep `.err` for
   `Reading of fields to be postprocessed` — TRUST lists available fields
   there.
5. Still stuck → `make module_debug` then `trust -gdb <case>` or
   `trust -valgrind <case>`.

## Reference (upstream)

- https://cea-trust-platform.readthedocs.io/en/v1.9.8/TRUST_Validation.html
- https://cea-trust-platform.readthedocs.io/en/v1.9.8/DevTutorials.html
