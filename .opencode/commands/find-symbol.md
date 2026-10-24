---
description: Look up a symbol definition in TRUST/TRIOCFD source indexes
---
Look up a symbol in the prebuilt TRUST/TRIOCFD codebase indexes.

Run:

    .opencode/scripts/find-symbol.sh $ARGUMENTS

If the index is missing, build it first:

    .opencode/scripts/index-codebases.sh

Options (pass through $ARGUMENTS): `--trust`/`--trio`/`--both`, `--kind KIND`,
`--context N`, `--count`. Example: `/find-symbol Objet_U --context 5`.

If no match is found, fall back to ripgrep over `../trust-code/src` and
`~/Development/triocfd-code/src` before concluding the symbol doesn't exist.
Do not modify the codebases. Report `file:line` and the prototype.