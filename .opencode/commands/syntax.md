---
description: Fast syntax-only compile of one C++ file
---
Load the `trust-lime-coding` skill for style/idiom context if a follow-up fix
is requested.
Run a fast syntax-only compile of $ARGUMENTS using the project's real compiler flags.

    bash .opencode/scripts/syntax-check.sh $1

This reuses the flags from `compile_commands.json`, runs `g++ -fsyntax-only`, and finishes in seconds without linking. It is the quickest diagnostic available, since OpenCode V2 has no LSP diagnostics yet.

Report the compiler errors and warnings verbatim and in order. If there are none, reply exactly: clean. Do not fix the file unless I ask.
