---
name: trust-LiMe environment
description: >
Set up and use the TRUST + trust-LiMe runtime environment: host detection
(laptop vs workstation), sourcing order for env_TRUST.sh / env_for_python.sh /
venv_trust / env_trust_LiMe.sh, the $exec guard, headroom proxy usage, and
the with-trust-env.sh wrapper. Use when a command needs the TRUST env, when
running validation or unit tests, or when debugging env-related failures.
---

# trust-LiMe environment

## Hosts

| Host | CPU | Storage | Notes |
|---|---|---|---|
| Laptop (T430s class) | 4 threads | HDD | Slowest; prefer `module_*` targets and generous timeouts. |
| Workstation (`matthieu-precisiont5610`) | Ryzen 5 5600X, 12 threads, 15 GiB | SSD root (`sda`), external HDD (`sdb`) | Faster; full kernel builds still long. |

## Sourcing order (must be this order)

```bash
source ../trust-code/env_TRUST.sh          # TRUST_ROOT, paths, make
source ../trust-code/env_for_python.sh     # Python/Jupyter env
source ~/Development/venv_trust/bin/activate   # venv_trust (Python deps)
source env_trust_LiMe.sh                  # sets $exec = .../trust-lime_opt
```

The `$exec` variable names the module binary that `trustutils` runs. **Guard
it** before any validation run:

```bash
[[ "$exec" == *trust-lime* ]] || { echo "WRONG exec: $exec" >&2; exit 1; }
```

A wrong `$exec` makes every case fail with `PB_MHD is not a recognized keyword`.

## Wrapper

Avoid repeating the four `source` lines — use the wrapper:

```bash
.opencode/scripts/with-trust-env.sh <command> [args...]
# e.g.
.opencode/scripts/with-trust-env.sh make module_semi_optim
.opencode/scripts/with-trust-env.sh bash -c 'make module_semi_optim && jupyter nbconvert ...'
```

Overrides: `TRUST_ROOT`, `VENV_TRUST` env vars.

## Waiting for builds

For long builds, run in background and poll:

```bash
.opencode/scripts/wait-build.sh [pattern] [poll_seconds]
# default pattern: 'make (optim|semi_optim|module_|check_|ctest_)'
```

## Headroom proxy

All model traffic goes through the local proxy `127.0.0.1:8787`
(`--mode cache`, 2 workers). It preserves the provider prefix cache (~100%
hit rate) — do **not** switch to `--mode token` (rewrites prior turns, busts
the cache). Measure with `headroom perf --hours 1`. The proxy itself adds no
token cost; `warming` must stay off (keep-alive requests count against the
budget). Nightly Hermes job runs on Gemini, not Zen, so scheduled work never
competes with interactive sessions.