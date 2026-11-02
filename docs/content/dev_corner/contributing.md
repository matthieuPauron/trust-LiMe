@page TrustLiMe_dc_contrib Contributing

TRUST-LiMe is a baltik module: it reuses the TRUST kernel and only ships its
own sources (`src/`), tests (`tests/`, `share/Validation/`) and documentation.

## Conventions

- C++20, `-Werror` with `-Wall -Wextra -Wconversion -Wshadow
  -Woverloaded-virtual -Wnon-virtual-dtor -pedantic`. Warnings are errors.
- Kernel headers are included **with** `.h`: `#include <Convection_Diffusion_Temperature.h>`.
- `.clang-format` is provided; auto-format on write is off.
- Reuse kernel base classes rather than reimplementing (see
  `docs/content/dev_corner/kernel_reuse.md`).

## Validation rules

- All validation cases are **sequential** (no `Decouper`/`Scatter`/`partition()`,
  no `nbProcs>1`), except the explicitly approved parallel paths.
- One form per notebook; `.ipynb` at form root only.
- Run `check-validation.sh` before submitting.

## Citation of external sources

Any work derived from an outside paper, project, or third-party code **must**
be duly cited. This is of paramount importance: every validation case,
analytic reference, mesh, coefficient, or algorithm borrowed from the
literature or from another project must name its source (author, year,
title, and where applicable the equation/figure/table number) in the
notebook, the data file header, and the relevant documentation page.
Prefer primary sources; when only a secondary summary is available, cite
both. Do not paraphrase a published result without attribution.

## Commit hygiene

Group commits logically (one concern per commit); see the `trust-lime-git` skill
for commit-grouping guidance. Never push from agent sessions.