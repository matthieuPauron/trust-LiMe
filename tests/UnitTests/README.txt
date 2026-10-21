In this directory we place small unit tests for trust-LiMe.

They follow the TRUST kernel convention (trust-code/tests/UnitTests): small,
fast, GoogleTest based, and they pin *business logic* rather than end-to-end
behaviour.

The CMakeLists found in the kernel's tests/UnitTests is not reproduced here:
baltik does not generate a "unit" target for a module, and the kernel's file
relies on CMake variables (libtrio, libdeps, syslib) that only exist inside the
baltik configure environment. Instead the driver script below reuses the two
things baltik already knows how to produce:

  * the compile flags -- from compile_commands.json, via
    .opencode/scripts/syntax-check.sh (SYNTAX_CHECK_PRINT_FLAGS=1);
  * the link line     -- from build/src/exec_opt/CMakeFiles/exe.dir/link.txt,
    so the test binary is linked exactly like the real trust-lime executable.

Running
-------

    .opencode/scripts/unit-tests.sh                 # full suite
    .opencode/scripts/unit-tests.sh --arith-only    # arithmetic only
    .opencode/scripts/unit-tests.sh --no-run        # build only
    .opencode/scripts/unit-tests.sh --gtest_filter=InductionStretchingKernel.*

Any extra argument is forwarded to the test binary, so the usual gtest flags
(--gtest_filter, --gtest_list_tests, ...) work.

--arith-only builds unit_induction_kernel.cpp and
unit_mhd_posttraitement_kernel.cpp alone. They need nothing but the TRUST
array type, so they run as soon as the micro + numeric kernels exist
(make micro_kernel_semi_optim numeric_kernel_semi_optim). The other suites
instantiate Convection_Diffusion_Magnetic_field,
Terme_Source_Induction_stretching and Terme_Source_Lorentz, which derive
from classes of the *standard* kernel and therefore need the full build
(make optim).

Contents
--------

  main_gtest.cpp                 full main(): Kokkos + Comm_Group_Noparallel +
                                 PE_Groups + statistics, mirroring the kernel.
  main_gtest_lite.cpp            arithmetic-only main(): Kokkos only. Pulling in
                                 PE_Groups here drags Comm_Group_MPI into the
                                 static link and breaks archive ordering.
  unit_magnetic_field.cpp        equation and problem contracts.
  unit_induction_source.cpp      data-file contract of the M1 source object.
  unit_lorentz_source.cpp        data-file contract of the Lorentz source:
                                 primary keyword, mu0 override/fallback,
                                 problem binding, and the error paths
                                 (death tests).
  unit_induction_kernel.cpp      arithmetic of S = (b.grad) u.
  unit_mhd_posttraitement_kernel.cpp  arithmetic of the post-treatment fields.

Writing a new test
------------------

A unit test here should need no mesh, no time scheme and no solver. Two tricks
from the kernel make that possible:

  EChaineJDD + Interprete_bloc parse a data block straight from a string, so
  keywords and error paths can be exercised without a .data file (see
  trust-code/tests/UnitTests/unit_params.cpp);

  EXPECT_DEATH covers the error paths, since Process::exit() terminates the
  process.

If a piece of logic is hard to reach from a test, factor it into a small
header-only function as was done for src/Induction_stretching_kernel.h: the
gradient index convention is the most error-prone part of the module and is
now covered without a single TRUST run.