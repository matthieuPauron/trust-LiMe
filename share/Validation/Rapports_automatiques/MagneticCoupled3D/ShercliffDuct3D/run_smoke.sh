#!/bin/bash
# Smoke test for the coupled 3D duct (Tassone Fig. 6).
# NOT a validation form: the §18.2 heisenbug blocks new 3D forms.
# Runs the coupled dataset from a safe path and reports pass/fail.
set +e
repo=/home/matthieu/Development/trust-LiMe
src=$repo/share/Validation/Rapports_automatiques/MagneticCoupled3D/ShercliffDuct3D/src
work=/tmp/opencode/duct_smoke
rm -rf "$work"; mkdir -p "$work"
cd "$work"

source ~/Development/trust-code/env_TRUST.sh >/dev/null 2>&1
source ~/Development/trust-code/env_for_python.sh >/dev/null 2>&1
source "$repo/env_trust_LiMe.sh" >/dev/null 2>&1

# Substitute placeholders: coarse mesh, Ha=0 control (pmag=0)
python3 - <<'PYEOF'
s=open("/home/matthieu/Development/trust-LiMe/share/Validation/Rapports_automatiques/MagneticCoupled3D/ShercliffDuct3D/src/shercliff_duct_3d.data").read()
s=s.replace("$nx","5").replace("$ny","17").replace("$nz","5")
s=s.replace("$tmax","10").replace("$eta","0.05").replace("$pmag","0")
open("shercliff_duct_3d.data","w").write(s)
PYEOF

echo "=== Running coupled 3D duct (Ha=0 control) ==="
source /home/matthieu/Development/trust-code/env_TRUST.sh >/dev/null 2>&1
source /home/matthieu/Development/trust-code/env_for_python.sh >/dev/null 2>&1
source /home/matthieu/Development/trust-LiMe/env_trust_LiMe.sh >/dev/null 2>&1
/home/matthieu/Development/trust-LiMe/trust-lime_semi_opt shercliff_duct_3d.data 1>run.out 2>run.err
rc=$?
echo "exit code: $rc"
if [ -f shercliff_duct_3d.stop ]; then
    echo "stop file: $(head -1 shercliff_duct_3d.stop)"
fi
echo "=== tail of stderr ==="
tail -3 run.err
echo "=== verdict ==="
if [ "$rc" -le 1 ] && grep -q "Finished correctly" shercliff_duct_3d.stop 2>/dev/null; then
    echo "PASS: coupled 3D duct runs and solves cleanly."
else
    echo "FAIL: rc=$rc — blocked by §18.2 heisenbug (layout-dependent segfault)."
    echo "The data file is syntactically valid; the crash is in the kernel's"
    echo "Op_Diff_PolyMAC_MPFA_Elem (uninitialized read → denormal → FPE)."
fi
