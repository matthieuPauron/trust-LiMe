@page TrustLiMe_dc_build Building the baltik

TRUST-LiMe is built through the baltik framework on top of an already-compiled
TRUST kernel. Always run from the repository root.

| Command | Meaning |
|---|---|
| `make module_semi_optim` | **Default.** Module only, `-O3`, asserts on. |
| `make module_optim` / `module_debug` | Module, optimized / debug. |
| `make optim` / `semi_optim` / `debug` | Full build (kernel + module). Slow. |
| `make module_custom` | Module with `project.cfg` flags (sanitizers). |
| `make check_optim` / `ctest_optim` | Baltik / ctest suites. |
| `make docs` | Build this documentation. |

Source the environment first:

```bash
source env_trust_LiMe.sh
# or
bash .opencode/scripts/with-trust-env.sh <command>
```

For host detection, sanitizers and the syntax-check loop, see `AGENTS.md` and
the `trust-lime-coding` / `trust-lime-env` skills.