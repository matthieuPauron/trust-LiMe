---
name: trust-LiMe probe analysis
description: >
  Read and analyse TRUST .son probe files in validation notebooks: SonFile API,
  effective probe coordinates from headers, analytic comparisons. Use when
  writing or debugging notebook check cells, or comparing a run to an analytic
  solution.
---

# trust-LiMe probe analysis

## Reading a probe

```python
from trustutils.files import SonFile
import numpy as np, os

son = SonFile(path, None)          # the single-arg form is NOT supported
entries = son.getEntries()         # one per probe point / component
assert entries, f"no probe entry in {path}"
t, v = son.getValues(entries[0])   # t: time array, v: values
t = np.asarray(t, dtype=float)
v = np.asarray(v, dtype=float)
```

Probe files land in `src/` next to the dataset; copy them into
`BUILD_DIRECTORY` after `runCases()` (see `trust-lime-notebooks`) or read
directly from `src/`.

## Effective probe coordinates

TRUST writes the actual cell-centre coordinates in the `.son` header:

```
# Temps x= 0.35 y= 0.45 z= 0.5
```

Parse them and evaluate the analytic solution there — the mesh may snap probes
to cell centres that differ from the nominal coordinates:

```python
def probe_pos(path):
    with open(path) as f:
        for line in f:
            if line.startswith("# Temps"):
                toks = line.replace("=", " = ").split()
                return (float(toks[toks.index("x") + 2]),
                        float(toks[toks.index("y") + 2]))
```

## Comparing to an analytic solution

- Compute the expected value at the *effective* coordinate, not the nominal one.
- Use `np.allclose` with a tolerance that reflects the discretisation (the
  project typically uses 5 % for balance checks, 1e-10 for frozen fields).
- For multi-point probes (`Points N ...`), iterate `getEntries()` and check
  each.

## Pitfalls

- `SonFile(path)` (single arg) raises `TypeError`; always pass `None`.
- `.son` files are written next to the dataset in `src/`, not in
  `BUILD_DIRECTORY` — copy them or read from `src/`.
- A leftover `.son` from a previous run can mask a failed case; clean stale
  artifacts before re-running.
