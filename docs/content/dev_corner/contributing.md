@page TrustLiMe_dc_contrib Contributing

TRUST-LiMe is a baltik module: it reuses the TRUST kernel and only ships its
own sources (`src/`), tests (`tests/`, `share/Validation/`) and documentation.

## Conventions

- C++20, `-Werror` with `-Wall -Wextra -Wconversion -Wshadow
  -Woverloaded-virtual -Wnon-virtual-dtor -pedantic`. Warnings are errors.
- Kernel headers are included **with** `.h`: `#include <Convection_Diffusion_Temperature.h>`.
- `.clang-format` is provided; auto-format on write is off.
- Reuse kernel base classes rather than reimplementing (see
  `KERNEL_REUSE_NOTES.md`).

## Validation rules

- All validation cases are **sequential** (no `Decouper`/`Scatter`/`partition()`,
  no `nbProcs>1`), except the explicitly approved parallel paths.
- One form per notebook; `.ipynb` at form root only.
- Run `check-validation.sh` before submitting.

## Commit hygiene

Group commits logically (one concern per commit); see the `trust-lime-git` skill
for commit-grouping guidance. Never push from agent sessions.