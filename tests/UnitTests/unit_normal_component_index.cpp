/****************************************************************************

 * trust-LiMe — unit tests for the conducting-wall normal-component helper.
 ****************************************************************************/

#pragma GCC diagnostic push
#if __GNUC__ < 9
#pragma GCC diagnostic ignored "-Wsuggest-override"
#endif
#include <gtest/gtest.h>
#pragma GCC diagnostic pop

#include <Normal_component_index.h>

#include <cmath>
#include <vector>

TEST(NormalComponentIndex, InvalidInputsReturnMinusOne)
{
  EXPECT_EQ(normal_component_index(nullptr, 2, 2), -1);
  EXPECT_EQ(normal_component_index(nullptr, 3, 3), -1);
  double n[2] = {1., 0.};
  EXPECT_EQ(normal_component_index(n, 0, 2), -1);
  EXPECT_EQ(normal_component_index(n, 2, 0), -1);
  EXPECT_EQ(normal_component_index(n, 2, -1), -1);
}

TEST(NormalComponentIndex, ScalarModelReturnsMinusOne)
{
  double n2[2] = {1., 0.};
  EXPECT_EQ(normal_component_index(n2, 2, 1), -1);
  double n3[3] = {0., 0., 1.};
  EXPECT_EQ(normal_component_index(n3, 3, 1), -1);
  EXPECT_EQ(normal_component_index(n3, 3, 2), -1);
}

TEST(NormalComponentIndex, MismatchedCountsReturnMinusOne)
{
  double n[2] = {1., 0.};
  EXPECT_EQ(normal_component_index(n, 2, 3), -1);
}

TEST(NormalComponentIndex, TwoDVectorPicksNormal)
{
  double nx[2] = {1., 0.};
  EXPECT_EQ(normal_component_index(nx, 2, 2), 0);
  double ny[2] = {0., 1.};
  EXPECT_EQ(normal_component_index(ny, 2, 2), 1);
  double diag[2] = {0.7071, 0.7071};
  EXPECT_EQ(normal_component_index(diag, 2, 2), 0);
}

TEST(NormalComponentIndex, ThreeDVectorPicksNormal)
{
  double nx[3] = {1., 0., 0.};
  EXPECT_EQ(normal_component_index(nx, 3, 3), 0);
  double ny[3] = {0., 1., 0.};
  EXPECT_EQ(normal_component_index(ny, 3, 3), 1);
  double nz[3] = {0., 0., 1.};
  EXPECT_EQ(normal_component_index(nz, 3, 3), 2);
  double n[3] = {0.1, 0.9, 0.3};
  EXPECT_EQ(normal_component_index(n, 3, 3), 1);
}

TEST(NormalComponentIndex, VectorOverload)
{
  std::vector<double> n = {0., 0., 1.};
  EXPECT_EQ(normal_component_index(n, 3), 2);
  std::vector<double> empty;
  EXPECT_EQ(normal_component_index(empty, 3), -1);
  std::vector<double> tie = {0.5, 0.5, 0.1};
  EXPECT_EQ(normal_component_index(tie, 3), 0);
}
