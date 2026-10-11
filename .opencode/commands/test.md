---
description: Run the trust-LiMe baltik/ctest suite
---
Run the TRUST-LiMe test suite.

Default (optimized):

    make ctest_optim

If `$ARGUMENTS` is non-empty, use it as the target, e.g. `/test ctest_debug`, `/test ctest_rerun_failed_optim`, `/test check_optim`.

This can be slow; use a generous timeout. Summarize pass/fail per test, and paste the failing test names with their output. Do not attempt fixes unless I ask.
