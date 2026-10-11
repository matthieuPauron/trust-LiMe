/****************************************************************************
 * trust-LiMe — unit tests for the arithmetic of the M1 source term.
 *
 * S_i = sum_j b_j * (d u_i / d x_j)
 *
 * The contraction lives in the header-only induction_stretching_ajouter() so
 * that it can be checked without a mesh, a time scheme and a linear solver.
 * This matters because the gradient component layout (component i*dim+j is
 * d u_i / d x_j) is easy to transpose and still gives plausible numbers for a
 * diagonal gradient.
 *
 * These tests need nothing but the TRUST array type, so they can be built and
 * run as soon as the micro + numeric kernels exist (see README.txt).
 *
 * See tests/UnitTests/README.txt for how to build and run.
 ****************************************************************************/

#pragma GCC diagnostic push
#if __GNUC__ < 9
#pragma GCC diagnostic ignored "-Wsuggest-override"
#endif
#include <gtest/gtest.h>
#pragma GCC diagnostic pop

#include <Induction_stretching_kernel.h>
#include <TRUSTTab.h>

namespace
{
// The 2D layout produced by Discret_Thyd::grad_u:
//   component 0 = du/dx   component 1 = du/dy
//   component 2 = dv/dx   component 3 = dv/dy
constexpr int kDim = 2;

DoubleTab tab_from(int nb_elem, int nb_comp, const double *values)
{
  DoubleTab tab(nb_elem, nb_comp);
  for (int ie = 0; ie < nb_elem; ie++)
    for (int c = 0; c < nb_comp; c++)
      tab(ie, c) = values[ie * nb_comp + c];
  return tab;
}
} // namespace

/*! With the identity gradient (u = (x, y)), S = b: the source reproduces the
 *  field component-wise. This is the exp(t) growth checked end-to-end by the
 *  source_stretching.data validation case.
 */
TEST(InductionStretchingKernel, IdentityGradientReproducesTheField)
{
  const double b_vals[] = { 2.0, -3.0 };
  const double gu_vals[] = { 1.0, 0.0, 0.0, 1.0 };  // du/dx du/dy dv/dx dv/dy

  const DoubleTab b = tab_from(1, 2, b_vals);
  const DoubleTab gu = tab_from(1, 4, gu_vals);

  DoubleTab secmem(1, 2);
  secmem = 0.;
  induction_stretching_ajouter(b, gu, 2, kDim, secmem);

  EXPECT_DOUBLE_EQ(secmem(0, 0), b(0, 0));
  EXPECT_DOUBLE_EQ(secmem(0, 1), b(0, 1));
}

/*! A deliberately asymmetric gradient pins the index layout.
 *
 *  Correct (component i*dim+j is d u_i/d x_j):
 *      S_0 = b0*du/dx + b1*du/dy = 2*1 + 3*10    =   32
 *      S_1 = b0*dv/dx + b1*dv/dy = 2*100 + 3*1000 = 3200
 *  A transposed layout would give 302 and 3020 instead.
 */
TEST(InductionStretchingKernel, UsesTheRowMajorKernelGradientLayout)
{
  const double b_vals[] = { 2.0, 3.0 };
  const double gu_vals[] = { 1.0, 10.0, 100.0, 1000.0 };

  const DoubleTab b = tab_from(1, 2, b_vals);
  const DoubleTab gu = tab_from(1, 4, gu_vals);

  DoubleTab secmem(1, 2);
  secmem = 0.;
  induction_stretching_ajouter(b, gu, 2, kDim, secmem);

  EXPECT_DOUBLE_EQ(secmem(0, 0), 32.0);
  EXPECT_DOUBLE_EQ(secmem(0, 1), 3200.0);
}

/*! With `nb_composantes 1` only one magnetic component is transported, so only
 *  the derivatives along that direction may contribute: S_i = b0 * d u_i/d x.
 */
TEST(InductionStretchingKernel, SingleComponentOnlyUsesThatDirection)
{
  const double b_vals[] = { 5.0 };
  const double gu_vals[] = { 1.0, 10.0, 100.0, 1000.0 };

  const DoubleTab b = tab_from(1, 1, b_vals);
  const DoubleTab gu = tab_from(1, 4, gu_vals);

  DoubleTab secmem(1, 2);
  secmem = 0.;
  induction_stretching_ajouter(b, gu, 1, kDim, secmem);

  EXPECT_DOUBLE_EQ(secmem(0, 0), 5.0 * 1.0);
  EXPECT_DOUBLE_EQ(secmem(0, 1), 5.0 * 100.0);
}

/*! The source is a right-hand-side *contribution*: it must accumulate into the
 *  existing secmem (which already carries mass + convection + diffusion), not
 *  overwrite it.
 */
TEST(InductionStretchingKernel, AccumulatesIntoTheExistingRightHandSide)
{
  const double b_vals[] = { 2.0, 3.0 };
  const double gu_vals[] = { 1.0, 10.0, 100.0, 1000.0 };

  const DoubleTab b = tab_from(1, 2, b_vals);
  const DoubleTab gu = tab_from(1, 4, gu_vals);

  DoubleTab secmem(1, 2);
  secmem(0, 0) = 1000.0;
  secmem(0, 1) = -1000.0;
  induction_stretching_ajouter(b, gu, 2, kDim, secmem);

  EXPECT_DOUBLE_EQ(secmem(0, 0), 1032.0);
  EXPECT_DOUBLE_EQ(secmem(0, 1), 2200.0);
}

/*! A constant velocity has a zero gradient, hence a zero stretching term:
 *  this is the "u = 0" case that must not perturb a pure diffusion run.
 */
TEST(InductionStretchingKernel, ZeroGradientLeavesTheRightHandSideUntouched)
{
  const double b_vals[] = { 2.0, 3.0 };
  const double gu_vals[] = { 0.0, 0.0, 0.0, 0.0 };

  const DoubleTab b = tab_from(1, 2, b_vals);
  const DoubleTab gu = tab_from(1, 4, gu_vals);

  DoubleTab secmem(1, 2);
  secmem(0, 0) = 7.0;
  secmem(0, 1) = 8.0;
  induction_stretching_ajouter(b, gu, 2, kDim, secmem);

  EXPECT_DOUBLE_EQ(secmem(0, 0), 7.0);
  EXPECT_DOUBLE_EQ(secmem(0, 1), 8.0);
}

/*! The contraction is element-wise: a per-cell variation of b must produce a
 *  matching per-cell variation of the source.
 */
TEST(InductionStretchingKernel, IsAppliedElementWise)
{
  // b = [ (1,2), (10,20) ], identity gradient => S = b on each cell.
  const double b_vals[] = { 1.0, 2.0, 10.0, 20.0 };
  const double gu_vals[] = { 1.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0, 1.0 };

  const DoubleTab b = tab_from(2, 2, b_vals);
  const DoubleTab gu = tab_from(2, 4, gu_vals);

  DoubleTab secmem(2, 2);
  secmem = 0.;
  induction_stretching_ajouter(b, gu, 2, kDim, secmem);

  EXPECT_DOUBLE_EQ(secmem(0, 0), 1.0);
  EXPECT_DOUBLE_EQ(secmem(0, 1), 2.0);
  EXPECT_DOUBLE_EQ(secmem(1, 0), 10.0);
  EXPECT_DOUBLE_EQ(secmem(1, 1), 20.0);
}

/*! A vanishing magnetic field must give a vanishing source, whatever the
 *  gradient: sanity check on the linear dependence on b.
 */
TEST(InductionStretchingKernel, ZeroFieldGivesZeroSource)
{
  const double b_vals[] = { 0.0, 0.0 };
  const double gu_vals[] = { 1.0, 10.0, 100.0, 1000.0 };

  const DoubleTab b = tab_from(1, 2, b_vals);
  const DoubleTab gu = tab_from(1, 4, gu_vals);

  DoubleTab secmem(1, 2);
  secmem(0, 0) = 5.0;
  secmem(0, 1) = -5.0;
  induction_stretching_ajouter(b, gu, 2, kDim, secmem);

  EXPECT_DOUBLE_EQ(secmem(0, 0), 5.0);
  EXPECT_DOUBLE_EQ(secmem(0, 1), -5.0);
}