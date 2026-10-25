/****************************************************************************
 * trust-LiMe — unit tests for the turbulent magnetic closure (keyword
 * registration and defaults).  No mesh, no solver.
 ****************************************************************************/

#pragma GCC diagnostic push
#if __GNUC__ < 9
#pragma GCC diagnostic ignored "-Wsuggest-override"
#endif
#include <gtest/gtest.h>
#pragma GCC diagnostic pop

#include <Modele_turbulence_magnetique.h>
#include <Convection_Diffusion_K_Magnetic.h>
#include <Convection_Diffusion_Epsilon_Magnetic.h>
#include <Terme_Source_K_Magnetic.h>
#include <Terme_Source_Epsilon_Magnetic.h>

TEST(ModeleTurbulenceMagnetique, TypeIsRegisteredUnderItsDataFileKeyword)
{
  Modele_turbulence_magnetique m;
  EXPECT_STREQ(m.le_type(), "Modele_turbulence_magnetique");
}

TEST(ConvectionDiffusionKMagnetic, TypeIsRegisteredUnderItsDataFileKeyword)
{
  Convection_Diffusion_K_Magnetic e;
  EXPECT_STREQ(e.le_type(), "Convection_Diffusion_K_Magnetic");
}

TEST(ConvectionDiffusionEpsilonMagnetic, TypeIsRegisteredUnderItsDataFileKeyword)
{
  Convection_Diffusion_Epsilon_Magnetic e;
  EXPECT_STREQ(e.le_type(), "Convection_Diffusion_Epsilon_Magnetic");
}

TEST(TermeSourceKMagnetic, TypeIsRegisteredUnderItsDataFileKeyword)
{
  Terme_Source_K_Magnetic s;
  EXPECT_STREQ(s.le_type(), "Terme_Source_K_Magnetic");
}

TEST(TermeSourceEpsilonMagnetic, TypeIsRegisteredUnderItsDataFileKeyword)
{
  Terme_Source_Epsilon_Magnetic s;
  EXPECT_STREQ(s.le_type(), "Terme_Source_Epsilon_Magnetic");
}