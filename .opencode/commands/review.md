---
description: Numerics-focused review of the current changes
---
Review the current changes (run `git diff`; also read any files named in $ARGUMENTS). Do not edit anything.

Focus on correctness and numerical safety for an MHD solver:

- uninitialized or aliased memory, out-of-bounds indexing, integer overflow in cell/face loops;
- MHD discretization: sign conventions, boundary conditions, divergence (Gauss law) handling, symmetric vs non-symmetric matrix assembly;
- `-Wconversion` / `-Werror` hazards (implicit double/int narrowing, sign mismatch) common in this codebase;
- MPI assumptions (global vs local indexing, ghost cells);
- ownership/lifetime (`OWN_PTR` vs raw pointers).

Output a prioritized list: blocker / important / nit. Cite `file:line`. Suggest a fix but do not apply it.
