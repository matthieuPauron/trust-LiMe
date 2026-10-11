---
description: Build the Jupyter Book documentation
---
Build the trust-LiMe MyST / Jupyter Book documentation:

    source ~/Development/venv_trust/bin/activate
    cd documentation && jupyter book build --html

Report warnings and the resulting output directory. If `$ARGUMENTS` names a notebook, re-execute it first (see `/validate`). Do not commit generated build output.
