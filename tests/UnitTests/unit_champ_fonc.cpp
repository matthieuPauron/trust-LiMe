/****************************************************************************
 * trust-LiMe — unit tests for post-treatment fields (keyword registration).
 ****************************************************************************/

#pragma GCC diagnostic push
#if __GNUC__ < 9
#pragma GCC diagnostic ignored "-Wsuggest-override"
#endif
#include <gtest/gtest.h>
#pragma GCC diagnostic pop

#include <Champ_Fonc_MHD.h>
#include <Champ_Fonc_Eta_Turbulent.h>
#include <Champ_Fonc_Somme.h>

TEST(ChampFoncMHD, TypeIsRegisteredUnderItsDataFileKeyword)
{
  Champ_Fonc_MHD c;
  EXPECT_STREQ(c.le_type(), "Champ_Fonc_MHD");
}

TEST(ChampFoncEtaTurbulent, TypeIsRegisteredUnderItsDataFileKeyword)
{
  Champ_Fonc_Eta_Turbulent c;
  EXPECT_STREQ(c.le_type(), "Champ_Fonc_Eta_Turbulent");
}

TEST(ChampFoncSomme, TypeIsRegisteredUnderItsDataFileKeyword)
{
  Champ_Fonc_Somme c;
  EXPECT_STREQ(c.le_type(), "Champ_Fonc_Somme");
}