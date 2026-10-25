/****************************************************************************
 * trust-LiMe — unit tests for magnetic boundary conditions (keyword
 * registration).
 ****************************************************************************/

#pragma GCC diagnostic push
#if __GNUC__ < 9
#pragma GCC diagnostic ignored "-Wsuggest-override"
#endif
#include <gtest/gtest.h>
#pragma GCC diagnostic pop

#include <Paroi_isolante_magnetique.h>
#include <Paroi_conductrice_magnetique.h>

TEST(ParoiIsolanteMagnetique, TypeIsRegisteredUnderItsDataFileKeyword)
{
  Paroi_isolante_magnetique cl;
  EXPECT_STREQ(cl.le_type(), "paroi_isolante_magnetique");
}

// (paroi_conductrice_magnetique registration is covered in
//  unit_magnetic_field.cpp to avoid a duplicate test name.)