---
name: trust-LiMe keyword search
description: >
  Find TRUST/TRUST-LiMe keywords and their definitions: `trust -search`, the
  generated KeywordsReference page, and trustify `// XD` annotations. Use when
  you need the syntax, parents, or parameters of a TRUST keyword, or to check
  whether a keyword exists before using it in a .data file.
---

# trust-LiMe keyword search

## Fast lookup

```bash
# search the kernel's keyword database (run in the TRUST env)
.opencode/scripts/with-trust-env.sh bash -c 'trust -search "Convection_Diffusion_Magnetic_field"'
```

Prints the keyword, its parent class, and a short signature. Works for any
TRUST or baltik keyword.

## Generated reference

`make docs` (or `cd docs && make`) runs trustify and emits the
`KeywordsReference` page. After a build:

```bash
grep -n "Convection_Diffusion_Magnetic_field" docs/build/html/KeywordsReference.html | head
```

## trustify `// XD` annotations

Module keywords are documented in the `.cpp` via `// XD` tags (see
`trust-lime-coding`). Validate a dataset's keywords against them:

```bash
.opencode/scripts/with-trust-env.sh bash -c 'trustify check path/to/case.data'
```

## When to use which

- Unknown keyword / "does this exist?" -> `trust -search`.
- Need parameters or a runnable example -> `KeywordsReference` HTML.
- After editing `// XD` tags -> `trustify check` on a sample `.data`.

## Pitfalls

- `trust -search` requires `$TRUST_ROOT` (source the env or use the wrapper).
- The `KeywordsReference` page is only as fresh as the last `make docs`.
- Module keywords are invisible to `trust -search` until the module is
  linked; use `trustify check` against the module's annotations instead.
