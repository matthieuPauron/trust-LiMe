#!/usr/bin/env bash
# syntax-check.sh — fast syntax-only compile of a TRUST / TRUST-LiMe C++ file.
#
# Usage:  .opencode/scripts/syntax-check.sh <path/to/file.cpp|.h>
#
# Why: OpenCode V2 has no LSP runtime yet, so there are no inline diagnostics.
# The compiler is the best oracle we have. This reuses the exact flags recorded
# in compile_commands.json, swaps the build compiler for the system g++, drops
# -Werror (warnings stay visible but do not fail), and runs -fsyntax-only.
# Result: seconds instead of a full link, and errors on the exact file you edit.
#
# Notes:
#  - The per-build compile database (build/src/exec*/compile_commands.json) is
#    preferred: it holds the exact flags the module was built with. The
#    repo-root compile_commands.json only contains trust-code platform entries,
#    so for a trust-LiMe source we fall back to a representative platform TU and
#    add the module include dirs. The TRUST flag set is uniform, so this is
#    faithful.
#  - Stale "/home/matthieu/" paths are repaired on the fly, so this works even
#    before you fix the database.
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
file="${1:?usage: syntax-check.sh <file.cpp|.h>}"
[[ -f "$file" ]] || { echo "error: no such file: $file" >&2; exit 2; }
# Normalize to an absolute path.
file="$(cd "$(dirname "$file")" && pwd)/$(basename "$file")"

db="$repo_root/compile_commands.json"
# Prefer the per-build compile database -- the exact flags the module was
# compiled with -- over the (stale) platform snapshot in the repo root.
build_db="$(ls -t "$repo_root"/build/src/exec*/compile_commands.json 2>/dev/null | head -1)"
[[ -n "$build_db" ]] && db="$build_db"
[[ -f "$db" ]] || { echo "error: compile_commands.json not found at $db" >&2; exit 2; }

CXX="${CXX:-/usr/bin/g++}"

flags="$(
  DB="$db" FILE="$file" REPO="$repo_root" python3 - <<'PY'
import json, os, shlex

db     = os.environ["DB"]
target = os.path.abspath(os.environ["FILE"])
repo   = os.environ["REPO"]

try:
    entries = json.load(open(db))
except Exception:
    entries = []

NOISE = {"-v", "-###", "--version", "-dumpversion", "-dumpmachine",
         "-print-search-dirs", "-E", "-M", "-MM"}

def args_of(e):
    a = e.get("arguments")
    if a:
        return list(a)
    c = e.get("command")
    return shlex.split(c) if c else []

def include_count(a):
    return sum(1 for t in a if t.startswith("-I"))

base = None
# 1. Exact match for the file being checked.
for e in entries:
    if os.path.abspath(e.get("file", "")) == target:
        base = args_of(e)
        break
# 2. The richest representative TU, i.e. the one with the most -I flags.
#    The TRUST flag set is uniform, so the fullest include list is the safest.
if base is None and entries:
    best = max(entries, key=lambda e: include_count(args_of(e)))
    base = args_of(best)
base = base or ["g++"]

# The recorded compiler is usually TRUST's mpich wrapper
# ($TRUST/lib/src/LIBMPI/mpich/bin/mpicxx), which injects the MPI include dir
# implicitly.  This script swaps it for a plain compiler, so restore that
# include path explicitly, otherwise mpi.h is not found (e.g. OverlapDEC.hxx).
mpi_inc = []
comp = base[0]
if comp and ("mpicxx" in comp or "mpic++" in comp or "mpicc" in comp):
    cand = os.path.join(os.path.dirname(os.path.dirname(comp)), "include")
    if os.path.isdir(cand):
        mpi_inc.append("-I" + cand)

out = list(mpi_inc)
skip_next = False
for i, tok in enumerate(base):
    if i == 0:
        continue                      # drop the compiler / ccache wrapper
    if skip_next:
        skip_next = False
        continue
    if tok == "-c":
        continue
    if tok == "-o":
        skip_next = True
        continue
    if tok in NOISE:
        continue
    # drop input/output file operands
    if tok.endswith((".cpp", ".cc", ".cxx", ".c", ".C", ".o",
                     ".f", ".f90", ".F", ".F90", ".cu")):
        continue
    out.append(tok)

# Repair stale home paths so the include dirs resolve (the recorded build
# machine used a different home directory, in either direction).
home = os.environ.get("HOME", "/home/matthieu")
out = [t.replace("/home/matthieu-pauron/", home + "/").replace("/home/matthieu/", home + "/") for t in out]

# Module include dirs (trust-LiMe itself is not in the platform database),
# plus the TRUST aggregator include/ directory of headers.
trust_code = os.environ.get("TRUST_CODE") or os.path.join(os.path.dirname(repo), "trust-code")
out += [f"-I{repo}/src", f"-I{repo}/build/src", f"-I{trust_code}/include"]

# The platform database is stale: it records -std=c++17, while the kernels
# (and Kokkos headers they pull in) are really built with C++20. Force C++20.
out = [t for t in out if not t.startswith("-std=")]
out.append("-std=c++20")

# Keep warnings visible but non-fatal for a quick syntax probe.
out += ["-Wno-error"]

# Extra flags (include paths, ...) are appended last so callers can reach files
# the platform database knows nothing about, e.g. the GoogleTest headers needed
# by tests/UnitTests/*.cpp:
#   EXTRA_CXXFLAGS="-I/path/to/gtest/include" syntax-check.sh tests/UnitTests/unit_misc.cpp
extra = os.environ.get("EXTRA_CXXFLAGS", "").strip()
if extra:
    out += shlex.split(extra)

print(shlex.join(out))
PY
)"

if [[ -n "${SYNTAX_CHECK_VERBOSE:-}" ]]; then
  echo "+ $CXX -fsyntax-only $flags $file" >&2
fi

# Flag-provider mode: print the flags and exit. Lets other scripts (e.g.
# tests/unit-tests) reuse exactly the same flags instead of duplicating the
# compile_commands.json parsing above.
if [[ "${SYNTAX_CHECK_PRINT_FLAGS:-}" == "1" ]]; then
  # shellcheck disable=SC2086  # intentional word splitting of $flags
  printf '%s\n' $flags
  exit 0
fi

# shellcheck disable=SC2086  # intentional word splitting of $flags and $EXTRA_CXXFLAGS
exec "$CXX" -fsyntax-only $flags ${EXTRA_CXXFLAGS:-} "$file"
