#!/usr/bin/env bash
# unit-tests.sh — build and run the trust-LiMe GoogleTest unit tests.
#
# Usage:
#   .opencode/scripts/unit-tests.sh              # full suite (needs the full TRUST build)
#   .opencode/scripts/unit-tests.sh --arith-only # arithmetic only (needs micro+numeric kernels)
#   .opencode/scripts/unit-tests.sh --no-run     # build only
#
# Why not "make unit"?
#   baltik does not generate a unit-test target for a module, and the TRUST
#   kernel's own tests/UnitTests/CMakeLists.txt depends on variables that only
#   exist inside the baltik configure environment. This script therefore reuses
#   the two things baltik already knows how to produce:
#     - the compile flags  -> from compile_commands.json (via syntax-check.sh)
#     - the link line      -> from build/src/exec<mode>/CMakeFiles/exe.dir/link.txt
#   so the test binary is linked exactly like the real trust-lime executable.
#
# The two suites have different requirements:
#   unit_induction_kernel  only needs DoubleTab  -> micro + numeric kernels suffice
#   everything else        needs Source_base, Convection_Diffusion_Temperature_base
#                         (standard kernel)
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
trust_root="${TRUST_ROOT:-$(cd "$repo_root/.." && pwd)/trust-code}"
gtest_root="${TRUST_GTEST_ROOT:-$trust_root/lib/src/LIBGTEST}"
build_dir="${UNIT_TEST_BUILD_DIR:-$repo_root/build/unit_tests}"
mode="full"
run=1

for arg in "$@"; do
  case "$arg" in
    --arith-only) mode="arith" ;;
    --no-run)     run=0 ;;
    -h|--help)    sed -n '2,30p' "${BASH_SOURCE[0]}"; exit 0 ;;
    *) echo "unknown option: $arg" >&2; exit 2 ;;
  esac
done

[[ -d "$gtest_root/include/gtest" ]] || {
  echo "error: GoogleTest headers not found under $gtest_root/include" >&2
  echo "       set TRUST_GTEST_ROOT to the LIBGTEST installation" >&2
  exit 2
}

# Build flavour: TRUST builds either optim (_opt) or semi_optim (_semi_opt)
# archives and its baltik link.txt lives under build/src/exec<flavour>. Pick the
# flavour whose kernel archive exists (override with UNIT_TEST_SUFFIX, e.g.
# "UNIT_TEST_SUFFIX=_semi_opt" after a `make semi_optim`).
kernel_suffix="${UNIT_TEST_SUFFIX:-}"
if [[ -z "$kernel_suffix" ]]; then
  for cand in _opt _semi_opt; do
    if [[ -f "$trust_root/lib/TRUST_mpi${cand}.a" ]]; then
      kernel_suffix="$cand"
      break
    fi
  done
fi
if [[ -z "$kernel_suffix" ]]; then
  echo "error: no TRUST kernel archive found (neither _opt nor _semi_opt)" >&2
  echo "       build the kernel first:  cd $trust_root && make optim" >&2
  echo "       (or make semi_optim for a _semi_opt kernel)" >&2
  exit 2
fi

kernel_link="$trust_root/lib/TRUST_mpi${kernel_suffix}.a"
if [[ "$mode" == "arith" ]]; then
  # Minimal chain: the numeric kernel already contains DoubleTab and friends.
  kernel_link="$trust_root/lib/TRUST_mpi_numeric_kernel${kernel_suffix}.a $trust_root/lib/TRUST_mpi_micro_kernel${kernel_suffix}.a"
fi
[[ -f "${kernel_link%% *}" ]] || {
  echo "error: TRUST kernel library not built: ${kernel_link%% *}" >&2
  echo "       build the kernel first:  cd $trust_root && make optim" >&2
  echo "       (for --arith-only only the micro + numeric kernels are required)" >&2
  exit 2
}

# --- compile ----------------------------------------------------------------
mkdir -p "$build_dir"

if [[ "$mode" == "arith" ]]; then
  test_sources=(tests/UnitTests/unit_induction_kernel.cpp
                tests/UnitTests/unit_mhd_posttraitement_kernel.cpp)
  module_sources=()
  main_source=tests/UnitTests/main_gtest_lite.cpp
else
  test_sources=(tests/UnitTests/unit_magnetic_field.cpp
                tests/UnitTests/unit_induction_source.cpp
                tests/UnitTests/unit_lorentz_source.cpp
                tests/UnitTests/unit_induction_kernel.cpp
                tests/UnitTests/unit_mhd_posttraitement_kernel.cpp)
  module_sources=(src/Convection_Diffusion_Magnetic_field.cpp
                  src/Convection_Diffusion_K_Magnetic.cpp
                  src/Convection_Diffusion_Epsilon_Magnetic.cpp
                  src/Modele_turbulence_magnetique.cpp
                  src/Terme_Source_K_Magnetic.cpp
                  src/Terme_Source_Epsilon_Magnetic.cpp
                  src/Pb_Induction.cpp
                  src/Pb_MHD.cpp
                  src/Pb_MHD_Turbulent.cpp
                  src/MHD_Coupling.cpp
                  src/Terme_Source_Induction_stretching.cpp
                  src/Terme_Source_Lorentz.cpp
                  src/Paroi_isolante_magnetique.cpp
                  src/Champ_Fonc_MHD.cpp
                  src/Champ_Fonc_Somme.cpp
                  src/Champ_Fonc_Eta_Turbulent.cpp)
  main_source=tests/UnitTests/main_gtest.cpp
fi

# Reuse the exact TRUST flags, with the GoogleTest include path appended.
flags="$(cd "$repo_root" && EXTRA_CXXFLAGS="-I$gtest_root/include ${EXTRA_CXXFLAGS:-}" \
         SYNTAX_CHECK_PRINT_FLAGS=1 bash .opencode/scripts/syntax-check.sh src/Pb_MHD.cpp)"

objects=()
for src in "${module_sources[@]}" "${main_source}" "${test_sources[@]}"; do
  obj="$build_dir/$(basename "${src%.cpp}").o"
  if [[ "$src" -nt "$obj" ]]; then
    echo "  CXX $(basename "$src")"
    # shellcheck disable=SC2086  # flags are intentionally word-split
    (cd "$repo_root" && g++ $flags -c "$src" -o "$obj")
  fi
  objects+=("$obj")
done

# --- link -------------------------------------------------------------------
exe="$build_dir/unit_lime_gtest"
echo "  LD  $(basename "$exe")"

link_txt="$repo_root/build/src/exec${kernel_suffix}/CMakeFiles/exe.dir/link.txt"
link_line=""
if [[ -f "$link_txt" ]]; then
  # Reuse baltik's recorded link line: same compiler wrapper, same libraries.
  # Two substitutions only:
  #   1. the module objects are replaced by ours, *in place* -- objects must sit
  #      before the libraries, appending them at the end silently fails to
  #      resolve anything against static archives;
  #   2. the standard-kernel archive is swapped for the pair that exists when
  #      only micro + numeric have been built.
  link_line="$(cat "$link_txt")"
  # Splice the test objects in place of the baltik module objects recorded in
  # the link line (objects must precede the libraries). This replaces the whole
  # CMakeFiles/list_obj.dir/*.o set -- whatever its size, e.g. the 4th module
  # source Terme_Source_Induction_stretching.cpp.o added later -- with ours.
  link_line="$(awk -v objs="${objects[*]}" '
    BEGIN { inserted = 0 }
    { for (i = 1; i <= NF; i++) {
        # The baltik link line may list its objects either inline
        # (CMakeFiles/list_obj.dir/*.o) or through a CMake response file
        # (@.../objects1.rsp): both are the "module objects" slot.
        if ($i ~ /^CMakeFiles\/list_obj\.dir\/.*\.o$/ || $i ~ /^@.*\.rsp$/) {
          if (!inserted) { printf "%s ", objs; inserted = 1 }
          continue
        }
        printf "%s ", $i
      }
    }' <<< "$link_line")"
  link_line="${link_line//$trust_root\/lib\/TRUST_mpi${kernel_suffix}.a/$kernel_link}"
  # Redirect the output binary. The recorded path is extracted first: a glob such
  # as " -o [^ ]*" is unsafe here, since bash bracket expressions inside
  # ${var//pat/rep} can swallow the whole library list that follows.
  old_out="$(awk '{for (i = 1; i <= NF; i++) if ($i == "-o") { print $(i + 1); exit }}' "$link_txt")"
  if [[ -n "$old_out" ]]; then
    link_line="${link_line// -o $old_out / -o $exe }"
  fi
  # Same stale-home repair as syntax-check.sh: the recorded build machine used a
  # different home directory.
  link_line="${link_line//\/home\/matthieu\//$HOME/}"

  # Drop libraries that were never built (the recorded line is the *full* TRUST
  # line, but a partial kernel checkout only produced part of it). For
  # --arith-only this is expected: libVc.a, PETSc & co. belong to the standard
  # kernel and are not needed by the array-only tests.
  dropped=()
  kept=""
  for tok in $link_line; do
    case "$tok" in
      *.a|*.so|*.so.*)
        if [[ ! -e "$tok" ]]; then
          dropped+=("$(basename "$tok")")
          continue
        fi
        ;;
    esac
    kept+="$tok "
  done
  link_line="$kept"
  if [[ "${#dropped[@]}" -gt 0 ]]; then
    echo "  (link: ${#dropped[@]} library(ies) not built, dropped: ${dropped[*]})"
  fi

  # The bundled MPICH embeds hwloc with its XML backend: pulling some of its
  # archive members (the full kernel does) leaves undefined libxml2 references
  # that the recorded CMake line never names (the real exe gets the system
  # libxml2 through CMake's link environment). Add it when missing - harmless
  # when the driver already resolves it.
  if [[ "$link_line" != *xml2* ]]; then
    link_line+=" -lxml2"
  fi
fi

# The MPI wrapper compiled into the link line invokes $TRUST_ROOT/exec/ccache/g++,
# so TRUST_ROOT must be visible even when the module env script was not sourced.
export TRUST_ROOT="$trust_root" TRUST_GTEST_ROOT="$gtest_root"
[[ -n "${UNIT_TEST_DEBUG:-}" ]] && printf 'link: %s\n' "$link_line"

if [[ -n "$link_line" ]]; then
  (cd "$repo_root/build/src/exec${kernel_suffix}" && \
   eval "$link_line" -L"$gtest_root/lib" -lgtest)
else
  # Fallback for a tree that has not been configured yet: minimal library set.
  echo "  (no baltik link.txt found, using a minimal library set)"
  # shellcheck disable=SC2086
  g++ -o "$exe" "${objects[@]}" \
    $kernel_link \
    "$trust_root/lib/libmfft.a" \
    "$trust_root/lib/src/LIBKOKKOS/linux_opt/lib64/libkokkoscontainers.a" \
    "$trust_root/lib/src/LIBKOKKOS/linux_opt/lib64/libkokkoscore.a" \
    "$trust_root/lib/src/LIBMPI/mpich/lib/libmpi.a" \
    -L"$gtest_root/lib" -lgtest -lpthread -ldl -lxml2
fi

[[ "$run" == "1" ]] || { echo "built $exe (not run)"; exit 0; }

# --- run --------------------------------------------------------------------
echo
exec "$exe" "$@"