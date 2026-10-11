/****************************************************************************
 * trust-LiMe — unit test main().
 *
 * Mirrors trust-code/tests/UnitTests/main_gtest.cpp. TRUST objects expect the
 * parallel environment and the performance counters to be initialised before
 * anything constructs an Objet_U, so a plain RUN_ALL_TESTS() is not enough.
 *
 * See tests/UnitTests/README.txt for how to build and run.
 ****************************************************************************/

#pragma GCC diagnostic push
#if __GNUC__ < 9
#pragma GCC diagnostic ignored "-Wsuggest-override"
#endif
#include <gtest/gtest.h>
#pragma GCC diagnostic pop

#include <TRUSTArray.h>
#include <Comm_Group_Noparallel.h>
#include <PE_Groups.h>
#include <Perf_counters.cpp>

int main(int argc, char **argv)
{
  Kokkos::initialize();

  Comm_Group_Noparallel groupe_trio;
  PE_Groups::initialize(groupe_trio);
  statistics().set_nb_time_steps_elapsed(3);
#ifdef TRUST_USE_GPU
  init_device();
#endif

  ::testing::InitGoogleTest(&argc, argv);
  const int result = RUN_ALL_TESTS();

  Kokkos::finalize();

  return result;
}