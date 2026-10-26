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
