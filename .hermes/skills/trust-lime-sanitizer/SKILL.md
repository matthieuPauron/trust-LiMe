---
name: trust-LiMe sanitizer workflow
description: >
  Run ASan/UBSan on trust-LiMe: project.cfg flags, module_custom target,
  fftw prerequisite bypass, revert-after-session. Use when investigating
  memory/UB errors in the divergence-projection assembly or full-kernel
  interactions.
---

# trust-LiMe sanitizer workflow

## Enabling

1. Set **both** flags in `project.cfg` (pairs are required):

   ```
   cpp_flags = -fsanitize=address,undefined ...
   ld_flags  = -fsanitize=address,undefined ...
   ```

   Enabling only `cpp_flags` leaves instrumentation without the link-time
   runtime and breaks the build.

2. Reconfigure and build:

   ```bash
   ./configure && make module_custom
   ```

3. The fftw prerequisite test fails to link (prebuilt fftw lacks the sanitizer
   runtime). Bypass with:

   ```bash
   TRUST_SKIP_PREREQUISITE_TEST=1 make module_custom
   ```

   Record the bypass; do not silently drop the prerequisite check.

## Running

```bash
# heavy-build guard first
.opencode/scripts/check-heavy-build.sh   # must exit 0
```

- Run at night when possible; prefer background builds with generous timeouts.
- Never run two heavy builds concurrently.
- Revert `project.cfg` after the session (both flags).

## Parallelism and RAM (CRYSTAL CLEAR POLICY)

Sanitizer jobs (ASan/UBSan, `make module_custom`) are **allowed to use as much RAM as they want** and **must run with the least possible parallelism**:

- Parallelism: `make -j1` only — never `-j2` or higher. Time is abundant; RAM/threads are scarce, and parallel compiles have OOM-killed `cc1plus` in this cgroup (observed 2026-10-26 23:02 and 2026-10-28 19:19).
- RAM: a sanitizer build must NOT be subject to the Hermes worker cgroup's 4 GiB `MemoryMax` cap. The cron worker scope imposes that cap and will OOM-kill `cc1plus` even when host RAM is free (observed 2026-10-28 19:19: host had ~8 GB free, worker scope killed `cc1plus` at 425 MB VM). Therefore sanitizer jobs must run in an **uncapped context** (interactive session, where `memory.max = max`) or with the worker cap explicitly raised/unlimited. Never run a sanitizer build under a capped worker scope expecting it to survive.

## Heavy-build guard

```bash
TRUST_ALLOW_HEAVY_BUILD=1 .opencode/scripts/check-heavy-build.sh
```

On a laptop host, heavy builds are refused outright unless the override is set
after explicit user confirmation. Daytime (08:00-20:00) and low free RAM
(< ~6 GB) also block.

## Pitfalls

- A leftover sanitizer config contaminates subsequent normal builds — revert
  `project.cfg` in the same task.
- `make module_custom` is RAM-intensive; can exhaust RAM and crash the machine.
- Nightly cron runs cannot ask questions; all bypasses must be documented in
  the job prompt or a skill.
- **Worker cgroup cap:** the Hermes cron worker runs in a systemd scope with a
  ~4 GiB `MemoryMax`. A sanitizer build under that scope will be OOM-killed
  even with free host RAM. Sanitizer jobs must run uncapped (interactive
  session or raised cap) and with `make -j1`.
- **Unreachable by Hermes:** RAM-intensive tasks (sanitizer builds) cannot run
  under the cron worker. The backlog marks them deferred 'unreachable by
  Hermes (user runs manually)' and skips them; the user runs them manually in
  an uncapped context.
