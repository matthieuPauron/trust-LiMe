---
description: Full optimized build (TRUST kernel + trust-LiMe module)
---
Run a full optimized build:

    make optim

This rebuilds the TRUST kernel and the trust-LiMe module and takes a long time on this machine. Use a generous timeout.

If `$ARGUMENTS` is non-empty, use it as the make target instead, e.g. `/build-full debug`.

Report errors first, verbatim; then summarize warnings by file. Do not start fixing unless I ask.
