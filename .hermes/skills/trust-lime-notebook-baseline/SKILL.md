---
name: trust-LiMe notebook baseline (RULE harness)
description: >
  Green-baseline gate and RULE harness for notebook edits:
  notebook-baseline.sh execute|compare, plus the ASN-28 staging and failure
  triage ladder. Use before editing or validating a notebook.
---

# trust-LiMe notebook baseline (RULE harness)

## Green-baseline gate (mandatory before adding a case)

0. **Formatting check**: `.opencode/scripts/check-validation.sh` must be green.
1. **Green baseline**: `make module_semi_optim` (plus `make semi_optim` if
   module sources changed), then `.opencode/scripts/unit-tests.sh`. Then run
   every existing dataset in the target notebook's `src/` and require `exit 0`
   from each, from a clean run directory (delete stale `.stop`, `.out`, `.err`,
   `*.son`, `*.TU` first).
2. **Add the case**: copy an existing validated dataset, adapt it, run the new
   case alone, compare probes to the analytic/invariant reference before
   wiring it into a notebook.
3. **Only then** give the case its own notebook (one test case per notebook,
   R4) and run that notebook.

Applies equally to new unit tests in `tests/UnitTests/`.

## RULE harness

```bash
.opencode/scripts/notebook-baseline.sh execute <FormName>   # run + record baseline
.opencode/scripts/notebook-baseline.sh compare  <FormName>   # verify unchanged cells
```

Unchanged cells must print identically and baseline probes must be reproduced
exactly. Run the gate before `/validate`.

## ASN 28 staging

Verification first, then validation against analytical data, and only then
comparison to experiments. A case moves to the next stage once the previous
one is green — never add an experimental-comparison case as the first test of
a new feature.

## Failure triage ladder

1. Compile error -> `syntax-check.sh` on the edited file.
2. Unit test fails -> `--arith-only` to isolate micro/numeric kernels.
3. Dataset fails -> run it alone from a clean directory; inspect `<case>.err`.
4. Unknown post-processing field name -> grep `.err` for
   `Reading of fields to be postprocessed`.
5. Still stuck -> `make module_debug` then `trust -gdb <case>` or
   `trust -valgrind <case>`.

## Pitfalls

- A leftover `.stop` halts the solve early.
- All cases run sequentially — never pass a processor count outside an
  allowlisted `parallel_ok[]` form.
- A passing formatting check is not evidence that a validation computation
  passed.
