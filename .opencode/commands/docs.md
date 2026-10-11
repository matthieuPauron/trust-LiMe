---
description: Build the trust-LiMe documentation (Doxygen / baltik pipeline)
---
Build the trust-LiMe Doxygen documentation. Load the `trust-lime-docs` skill
for the full recipe.

    source env_trust_LiMe.sh
    cd docs && make          # or: make docs  (from the repository root)

Report:
- any warnings originating from `docs/content/` (the many warnings from the
  TRUST kernel sources and the trustify-generated keyword pages are pre-existing
  noise);
- the output path, `docs/build/html/index.html`.

Do not commit `docs/build/`.
