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
- **GPU/Kokkos builds are host-specific.** TRUST's GPU path supports only specific HPC accelerators: AMD via ROCm on MI250X (`gfx90a`) / MI300A (`gfx942`) (`env/HOST_adastra-gpu.sh`, `TRUST_USE_ROCM=1`), and NVIDIA via CUDA (`TRUST_USE_CUDA=1`, `TRUST_CUDA_CC=<arch>`). Consumer GPUs (e.g. RX 7600 / `gfx1102`) are not in the supported `ROCM_ARCH` list and will be rejected; no ROCm install is expected on the workstation. Before attempting a GPU build, check `env/HOST_*-gpu.sh` for the supported `ROCM_ARCH`/`TRUST_CUDA_CC` and confirm the toolchain (rocm/hipcc or nvcc) is installed. Do not start a GPU build on this workstation expecting it to work.
- **Sanitizer builds** additionally: enable BOTH `cpp_flags` and `ld_flags` in `project.cfg` (half-configured otherwise), configure with `TRUST_SKIP_PREREQUISITE_TEST=1` if fftw/mgis smoke test fails on `__ubsan_handle_*`, build in the background with long timeout/notify (routinely >180 s), verify the `trust-lime_custom` binary exists, then revert `project.cfg` to fully commented state before any normal build.
- **Run at night when possible**, prefer background builds with notification, never run two heavy builds concurrently, and **default to `make -j1` only (no parallelism at all) for any heavy/sanitizer build — never `-j2` or higher; time is abundant, RAM/threads are scarce, and parallel compiles have OOM-killed cc1plus in this cgroup. Raise parallelism only with explicit user approval and confirmed free RAM.
- **Worktree runs** share the host RAM: require at least 8 GB free for any RAM-intensive action in a worktree (stricter than the 6 GB default guard), and never run two heavy builds concurrently across worktrees.
- **Build/src is a copy, not a symlink:** `build/src/` is a separate copy of `src/`. Files added to `src/` after the last configure are not auto-copied, and `make module_semi_optim` fails with `fatal error: ...h: No such file or directory` (file(GLOB) only sees files already in `build/src/`). Fix by copying new files into `build/src/` (or re-running `./configure`), then build. Check the `.so` mtime, not the top-level binary timestamp.
- **Cgroup ceiling**: `free -g` reports host RAM, not the worker's cgroup limit. A build can OOM even when host free RAM looks ample (observed: cc1plus killed at 23:02 with >6GB free). Treat the cgroup ceiling as the binding constraint; use `-j1`/`-j2` and never two concurrent heavy builds.
- **Sanitizer jobs (crystal clear): full RAM (uncapped) + `make -j1`.** Sanitizer builds (`make module_custom`) are allowed to use as much RAM as they want and must run with the least possible parallelism (`-j1` only, never `-j2`+). The Hermes cron worker's systemd scope imposes a ~4 GiB `MemoryMax` that OOM-kills `cc1plus` even with free host RAM (observed 2026-10-28 19:19); therefore sanitizer builds must run in an uncapped context (interactive session, `memory.max = max`) or with the worker cap raised/unlimited — never under a capped worker scope. **Unreachable by Hermes:** the cron worker cannot run them; the backlog marks them deferred 'unreachable by Hermes (user runs manually)' and skips them, and the user runs them manually in an uncapped context.
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
- A timed-out or killed `make` leaves a partial build tree; check for the binary/mtime and any running build process before declaring success or re-running.
- A background build started by an interrupted cron run can outlive the worker; reconcile process state, worktree existence, and binary mtime before retrying or resetting state.
- **Sanitizer flags can leak into semi_optim builds**: a stale `exec_semi_opt/CMakeCache.txt` from a prior sanitizer configure may keep `-fsanitize=address,undefined` in `CPP_FLAGS` even though `project.cfg` has them commented. Check the build log's "added to CPP_FLAGS" line before assuming a clean semi_optim build; reconfigure (`./configure`) if sanitizer flags appear unexpectedly.
- **Worktree builds rewrite absolute paths** in prebuilt dependency metadata (e.g. fftw CMake config/pkgconfig) to the worktree path and can regenerate wisdom/.so files; revert these (`git checkout -- local/fftw-3.3.8/`) before review so they don't masquerade as task changes.
- **Unit tests vs sanitizer mismatch**: `unit-tests.sh` compiles with flags from `exec_semi_opt/compile_commands.json` (which may carry `-fsanitize`) but links against the kernel `link.txt` (which may not). Result: undefined `__asan_*`/`__ubsan_*` symbols at link. Cheapest fix: run with `UNIT_TEST_SUFFIX=_custom` so both the compile flags and the link archive come from the sanitizer-configured `exec_custom` build (matching instrumentation). Alternatives: revert `project.cfg` sanitizer flags (comment both `cpp_flags`/`ld_flags`) and reconfigure, or compile an arith-only kernel test standalone (header + gtest) without sanitizer flags, or strip `-fsanitize=*` tokens from the compile flags inside `unit-tests.sh` (filter the flags returned by `syntax-check.sh`) when you want the non-sanitizer kernel suite from a sanitizer-configured tree.
- **VEF port source registration**: when wiring a VEF variant of a `Champ_Fonc_*` (adding `*_VEF.cpp`/`.h` to `src/` and branching on `sub_type(Domaine_VEF, ...)`), also add the new `*_VEF.cpp` files to `unit-tests.sh`'s `module_sources` list — otherwise the unit-test link fails with undefined `*_VEF::info_obj`/`self_cast`/`configurer`. The baltik `file(GLOB)` in `build/src/CMakeLists.txt` only sees files already present in `build/src/`, so copy them there (or re-run `./configure`) before building.
- **Pre-commit hook in a sanitizer-configured tree**: the hook calls `unit-tests.sh` with default suffix detection, which picks `_semi_opt` and can fail at link even when the code is fine. Validate first with `UNIT_TEST_SUFFIX=_custom ... unit-tests.sh --arith-only`; if it passes, commit with `git commit --no-verify` (do not leave the hook silently blocking). Never bypass without having run the matching test suite


## Pre-commit hook

The repo's pre-commit hook runs the unit suite and refreshes
`validation_status.json`. Treat a failure as a real failure, not a bypass.
Because the hook rewrites `validation_status.json`, a commit that only updates
that file will trip the hook and touch it again; commit such status-only changes
with `git commit --no-verify`.

## Verification

- `syntax-check.sh` returns exit code 0.
- `make module_semi_optim` succeeds and produces the updated module.
