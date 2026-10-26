---
name: trust-lime-build-diagnostics
description: "Run fast build loops and diagnostic checks efficiently."
version: 0.1.0
author: Matthieu (matthieu), Hermes Agent
license: MIT
platforms: [linux]
metadata:
  hermes:
    tags: [trust-lime, build, diagnostics]
    related_skills: [trust-lime-coding]
---

# trust-lime-build-diagnostics Skill

This skill defines the preferred workflow for build loops and fast-path diagnostics on the trust-LiMe codebase, specifically optimized for the 4-thread HDD hardware constraints.

## When to Use

- After every meaningful edit to `src/` files.
- Before running a full `make` or validation loop.
- When you suspect a compilation error or type mismatch.

## How to Run

1. **Syntax Check (Fast Path)**:
   ```bash
   bash .opencode/scripts/syntax-check.sh <file>
   ```
   This reuses `compile_commands.json` flags and runs `g++ -fsyntax-only` in seconds.

2. **Module Build (Default Path)**:
   ```bash
   make module_semi_optim
   ```
   This rebuilds only the module with `-O3` and asserts enabled.

3. **Check Targets**:
   ```bash
   make help
   ```
   Use this to list available build targets and test suites.

## Pitfalls

- **Avoid clangd**: On this machine's HDD, full indexing of the TRUST kernel causes unacceptable latency; use the compiler-direct path above instead.
- **Avoid full `make`**: Avoid `make optim` or `make debug` for module-only changes, as they trigger full kernel recompilation.

## Resource-heavy builds (RAM guard)

`make module_custom` and full-kernel builds (`make optim`/`semi_optim`/`debug`) can exhaust RAM on either host (laptop or workstation) and crash the machine — observed. The repo's `AGENTS.md` carries the standing rule; this skill carries the workflow.

- **Never auto-start one during daytime (08:00–20:00 local) or when free RAM < ~6 GB** without explicit user confirmation.
- **Pre-check first**: run `.opencode/scripts/check-heavy-build.sh` (exits non-zero + reason unless `TRUST_ALLOW_HEAVY_BUILD=1`).
- **Host detection**: `check-heavy-build.sh` now classifies the host (workstation vs laptop by hostname/thread count); on a laptop-class (≤4-thread) host heavy builds are refused outright unless `TRUST_ALLOW_HEAVY_BUILD=1`. Do not bypass on the HDD laptop without explicit user confirmation.
- **Unit tests**: `unit-tests.sh` auto-detects the `_opt`/`_semi_opt` kernel suffix from available archives + link.txt (override with `UNIT_TEST_SUFFIX` if both exist).
- **Sanitizer builds** additionally: enable BOTH `cpp_flags` and `ld_flags` in `project.cfg` (half-configured otherwise), configure with `TRUST_SKIP_PREREQUISITE_TEST=1` if fftw/mgis smoke test fails on `__ubsan_handle_*`, build in the background with long timeout/notify (routinely >180 s), verify the `trust-lime_custom` binary exists, then revert `project.cfg` to fully commented state before any normal build.
- **Run at night when possible**, prefer background builds with notification, never run two heavy builds concurrently, and use `make -j1`/`-j2` to cap RAM if needed.
- The nightly backlog cron job carries the same guard in its prompt (defers RAM-heavy candidates during daytime/low-RAM and reports the deferral); cron runs cannot ask questions.

## Sanitizer builds (ASan/UBSan)

Use this mode to pin memory/UB bugs (e.g. the layout-dependent SIGFPE in `kernel_reuse.md` §18.2). Module-only (`make module_custom`) is the fast path; full `make custom` is slow.

1. **Enable both flags** in `project.cfg`: uncomment `cpp_flags` **and** `ld_flags` (the block says "uncomment both"). Enabling only `cpp_flags` compiles instrumented code but links without the sanitizer runtime — a half-configured build.
2. **Configure**: `.opencode/scripts/with-trust-env.sh ./configure`. If the `fftw-3.3.8` (or `mgis`) prerequisite test fails with `undefined reference to __ubsan_handle_*` / `__asan_report_*`, the prebuilt dependency is not instrumented — re-run with `TRUST_SKIP_PREREQUISITE_TEST=1 ... ./configure`. That failure is an environment artifact, not a code bug.
3. **Build in the background with a long timeout/notify** — `make module_custom` on this 4-thread HDD routinely exceeds 180 s. Do not repeat short foreground calls; they only re-enter a partial build.
4. **Run the sanitizer tests** once the custom binary exists; record findings in the validation report / `dev_corner` notes.
5. **Revert `project.cfg` to the fully commented state before any normal build** (`module_semi_optim`, etc.). Leaving sanitizer flags on silently slows and re-instruments every subsequent build.

Pitfalls:
- Leaving `cpp_flags` on while `ld_flags` stays off produces a build that compiles but may fail to link/run the sanitizer runtime — `grep` both flags before building.
- `TRUST_SKIP_PREREQUISITE_TEST=1` skips only the fftw/mgis smoke test; it does not skip the module build. Still confirm `trust-lime_custom` was actually produced.
- A timed-out `make` leaves a partial build tree; check for the binary/mtime before declaring success or re-running.


## Verification

- `syntax-check.sh` returns exit code 0.
- `make module_semi_optim` succeeds and produces the updated module.
