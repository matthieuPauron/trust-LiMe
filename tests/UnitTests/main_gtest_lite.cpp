/****************************************************************************
 * trust-LiMe — lightweight main() for the arithmetic-only unit test binary.
 *
 * The arithmetic tests need nothing but the TRUST array type, so they avoid the
 * parallel environment set-up done by main_gtest.cpp (Kokkos aside). Pulling
 * in PE_Groups/statistics() here drags Comm_Group_MPI into the static link and
 * changes archive ordering, which breaks an otherwise fine link.
 *
 * See tests/UnitTests/README.txt.
 ****************************************************************************/

#pragma GCC diagnostic push
#if __GNUC__ < 9
#pragma GCC diagnostic ignored "-Wsuggest-override"
#endif
#include <gtest/gtest.h>
#pragma GCC diagnostic pop

#include <Kokkos_Core.hpp>

int main(int argc, char **argv)
{
  Kokkos::initialize();

  ::testing::InitGoogleTest(&argc, argv);
  const int result = RUN_ALL_TESTS();

  Kokkos::finalize();

  return result;
}