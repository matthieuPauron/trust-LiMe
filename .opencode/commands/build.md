---
description: Compile the trust-LiMe module (optimized, module-only)
---
Build the TRUST-LiMe C++ module.

Run this with the shell tool from the repository root:

    make module_semi_optim

Guidance:
- `module_semi_optim` rebuilds only this module against the already-built TRUST kernel: the fast, day-to-day target (`-O3`, asserts on).
- If `$ARGUMENTS` is non-empty, use it as the make target instead, e.g. `/build module_optim` or `/build module_debug`.
- This machine (4 threads, HDD) is slow; use a generous timeout.
- Report compiler errors first, verbatim and in file order. Group warnings by file. Do not start fixing unless I ask.
- If the kernel build is missing, say so and suggest `/build-full`.
