/****************************************************************************
 * trust-LiMe — arithmetic tests for the independent magnetic k-epsilon model.
 *
 * The production implementation delegates these scalar formulas to a
 * header-only kernel, making the closure testable without a TRUST mesh or
 * solver.
 ****************************************************************************/

#pragma GCC diagnostic push
#if __GNUC__ < 9
#pragma GCC diagnostic ignored "-Wsuggest-override"
#endif
#include <gtest/gtest.h>
#pragma GCC diagnostic pop

#include <Magnetic_turbulence_closure_kernel.h>

TEST(MagneticTurbulenceKernel, EtaTMatchesClosureFormula)
{
  EXPECT_DOUBLE_EQ(magnetic_turbulence_eta_t(2., 4., 0.09, 1.5, 1.e-12, 1.e30), 0.06);
}

TEST(MagneticTurbulenceKernel, EtaTUsesEpsilonFloorAndZeroesNegativeK)
{
  EXPECT_DOUBLE_EQ(magnetic_turbulence_eta_t(2., 0., 0.5, 2., 0.25, 1.e30), 4.);
  EXPECT_DOUBLE_EQ(magnetic_turbulence_eta_t(-2., 1., 0.5, 1., 1.e-12, 1.e30), 0.);
}

TEST(MagneticTurbulenceKernel, EtaTRespectsCap)
{
  EXPECT_DOUBLE_EQ(magnetic_turbulence_eta_t(10., 1., 1., 1., 1.e-12, 3.), 3.);
}

TEST(MagneticTurbulenceKernel, KSourceHasProductionAndDissipationSigns)
{
  EXPECT_DOUBLE_EQ(magnetic_turbulence_k_source(5., 2.), 3.);
  EXPECT_DOUBLE_EQ(magnetic_turbulence_k_source(1., 2.), -1.);
}

TEST(MagneticTurbulenceKernel, EpsilonSourceMatchesClosureFormula)
{
  const double expected = 1.44 * (2. / 4.) * 3. - 1.92 * 2. * 2. / 4.;
  EXPECT_DOUBLE_EQ(magnetic_turbulence_epsilon_source(1.44, 1.92, 2., 4., 1.e-12, 1.e-20, 3.), expected);
}

TEST(MagneticTurbulenceKernel, EpsilonSourceAppliesKAndEpsilonFloors)
{
  const double expected = 1.5 * (0.2 / 0.5) * 2. - 2. * 0.2 * 0.2 / 0.5;
  EXPECT_DOUBLE_EQ(magnetic_turbulence_epsilon_source(1.5, 2., 0., 0., 0.2, 0.5, 2.), expected);
}

TEST(MagneticTurbulenceKernel, ProductionScalesWithEtaFieldNormalizationAndCurl)
{
  EXPECT_DOUBLE_EQ(magnetic_turbulence_production(2., 0.5, 9.), 4.5);
  EXPECT_DOUBLE_EQ(magnetic_turbulence_production(2., 0.5, 0.), 0.);
}

TEST(MagneticTurbulenceKernel, ProviderSelectionAcceptsEachSingleCompleteProvider)
{
  EXPECT_TRUE(magnetic_turbulence_provider_configuration_valid(false, false, false, false, false));
  EXPECT_TRUE(magnetic_turbulence_provider_configuration_valid(true, false, false, false, false));
  EXPECT_TRUE(magnetic_turbulence_provider_configuration_valid(false, true, true, false, false));
  EXPECT_TRUE(magnetic_turbulence_provider_configuration_valid(false, false, false, true, false));
  EXPECT_TRUE(magnetic_turbulence_provider_configuration_valid(false, false, false, false, true));
}

TEST(MagneticTurbulenceKernel, ProviderSelectionRejectsIncompleteOrConflictingProviders)
{
  EXPECT_FALSE(magnetic_turbulence_provider_configuration_valid(false, true, false, false, false));
  EXPECT_FALSE(magnetic_turbulence_provider_configuration_valid(false, false, true, false, false));
  EXPECT_FALSE(magnetic_turbulence_provider_configuration_valid(true, true, true, false, false));
  EXPECT_FALSE(magnetic_turbulence_provider_configuration_valid(false, true, true, false, true));
  EXPECT_FALSE(magnetic_turbulence_provider_configuration_valid(true, false, false, true, false));
}
