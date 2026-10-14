/****************************************************************************
 * trust-LiMe — unit tests for the arithmetic of the MHD post-treatment fields
 * (MHD_posttraitement_kernel.h).
 *
 * The formulas live in a header-only, framework-free kernel precisely so that
 * they can be checked without a mesh, a time scheme or a solver (see
 * tests/UnitTests/README.txt). The most error-prone piece is the element
 * gradient layout (g[i*dim+j] = du_i/dx_j) used for the vorticity: the tests
 * below pin every index/sign convention, including on 3D analytic fields.
 *
 * See tests/UnitTests/README.txt for how to build and run.
 ****************************************************************************/

#pragma GCC diagnostic push
#if __GNUC__ < 9
#pragma GCC diagnostic ignored "-Wsuggest-override"
#endif
#include <gtest/gtest.h>
#pragma GCC diagnostic pop

#include <MHD_posttraitement_kernel.h>
#include <cmath>

namespace
{
constexpr double kTol = 1e-14;
}

/*---------------------------------------------------------------------------
 * |b|^2 and the two b-only fields.
 *--------------------------------------------------------------------------*/
TEST(MHDPosttraitement, NormOnlySumsTheTransportedComponents)
{
  // nb_b = 1 (scalar model): components beyond nb_b must not contribute.
  const double b[3] = { 3., -4., 99. };
  EXPECT_DOUBLE_EQ(mhd_norme2_b(b, 1), 9.);
  EXPECT_DOUBLE_EQ(mhd_norme2_b(b, 2), 25.);
  EXPECT_DOUBLE_EQ(mhd_norme2_b(b, 3), 9. + 16. + 99. * 99.);
}

TEST(MHDPosttraitement, EnergieMagnetique)
{
  // e_m = |b|^2 / (2 mu0)
  const double b[2] = { 1., 2. };            // |b|^2 = 5
  EXPECT_DOUBLE_EQ(mhd_energie_magnetique(mhd_norme2_b(b, 2), 1.), 2.5);
  EXPECT_DOUBLE_EQ(mhd_energie_magnetique(mhd_norme2_b(b, 2), 2.), 1.25);
}

TEST(MHDPosttraitement, PulsationCyclotroniqueUsesChargeOverMass)
{
  // omega_c = (q/m) |b| ; default q/m = e/m_e (positron charge).
  const double b[1] = { 0.5 };
  EXPECT_DOUBLE_EQ(mhd_pulsation_cyclotronique(mhd_norme2_b(b, 1), 2.), 1.);
  const double b3[3] = { 3., -4., 0. };      // |b| = 5
  EXPECT_DOUBLE_EQ(mhd_pulsation_cyclotronique(mhd_norme2_b(b3, 3), 0.1), 0.5);
}

/*---------------------------------------------------------------------------
 * u.b and the Poynting vector.
 *--------------------------------------------------------------------------*/
TEST(MHDPosttraitement, CrossedHelicity)
{
  const double u[2] = { 3., 4. }, b[2] = { 1., 2. };
  EXPECT_DOUBLE_EQ(mhd_produit_scalar_u_b(u, b, 2, 2), 11.);
  // Scalar model: u.b = u_x b_0 only.
  EXPECT_DOUBLE_EQ(mhd_produit_scalar_u_b(u, b, 1, 2), 3.);
}

TEST(MHDPosttraitement, PoyntingIsB2uMinusUbB)
{
  // S = (|b|^2 u - (u.b) b) / mu0, uniform b = (1, 2), u = (3, 4), mu0 = 2:
  // |b|^2 = 5, u.b = 11 -> S = (5u - 11b)/mu0 = ((15-11)/2, (20-22)/2) = (2, -1).
  const double u[2] = { 3., 4. }, b[2] = { 1., 2. };
  double s[2] = { 0., 0. };
  mhd_vecteur_poynting(u, b, 2, 2, 2., s);
  EXPECT_NEAR(s[0], 2., kTol);
  EXPECT_NEAR(s[1], -1., kTol);

  // Ideal-MHD Poynting vector must be orthogonal to b:
  double skal[3];
  const double u3[3] = { 0.3, -1.2, 0.8 }, b3[3] = { -2., 0.4, 1.5 };
  mhd_vecteur_poynting(u3, b3, 3, 3, 1.4, skal);
  EXPECT_NEAR(skal[0] * b3[0] + skal[1] * b3[1] + skal[2] * b3[2], 0., kTol);
}

/*---------------------------------------------------------------------------
 * Curl from the element gradient layout and kinetic helicity.
 *--------------------------------------------------------------------------*/
TEST(MHDPosttraitement, Curl2DPicksTheRightGradientComponents)
{
  // g[i*2+j] = du_i/dx_j; g = (dudx, dudy, dvdx, dvdy) = (1, 2, 3, 4)
  // curl_z = dv/dx - du/dy = 3 - 2 = 1.
  const double g[4] = { 1., 2., 3., 4. };
  double w[1] = { 0. };
  mhd_curl_u_elem(g, 2, w);
  EXPECT_DOUBLE_EQ(w[0], 1.);
}

TEST(MHDPosttraitement, Curl3DOnALinearField)
{
  // u = (y, z, x) -> grad u = [[0,1,0],[0,0,1],[1,0,0]]
  //           (row-major: g[0]=dudy? no: g[i*3+j]=du_i/dx_j)
  //   du_x/dy = 1, du_y/dz = 1, du_z/dx = 1, all other comps 0
  //   => curl u = (du_z/dy? -> 0 - 1 = -1, du_x/dz - du_z/dx = 0-1 = -1? ...)
  const double g[9] = { 0., 1., 0., 0., 0., 1., 1., 0., 0. };
  double w[3] = { 0., 0., 0. };
  mhd_curl_u_elem(g, 3, w);
  // w_x = du_z/dy - du_y/dz = 0 - 1 = -1
  // w_y = du_x/dz - du_z/dx = 0 - 1 = -1
  // w_z = du_y/dx - du_x/dy = 0 - 1 = -1
  EXPECT_DOUBLE_EQ(w[0], -1.);
  EXPECT_DOUBLE_EQ(w[1], -1.);
  EXPECT_DOUBLE_EQ(w[2], -1.);

  // h_k = u.(curl u) = -(x+y+z) -> at (0.5, 0.5, 0.5): -1.5
  const double u[3] = { 0.5, 0.5, 0.5 };
  EXPECT_DOUBLE_EQ(mhd_helicite_cinetique(u, w, 3), -1.5);
}

TEST(MHDPosttraitement, KineticHelicityVanishesIn2D)
{
  // In 2D curl u is perpendicular to the plane and u is in-plane -> h_k = 0.
  const double u[2] = { 17., -3.2 };
  const double w[1] = { -0.7 };
  EXPECT_DOUBLE_EQ(mhd_helicite_cinetique(u, w, 2), 0.);
}

/*---------------------------------------------------------------------------
 * Dimensionless numbers.
 *--------------------------------------------------------------------------*/
TEST(MHDPosttraitement, MagneticReynolds)
{
  // Rm = |u| L / eta ; u = (3, 4), L = 0.1, eta = 2 -> 5 * 0.5 = 0.25
  const double u[2] = { 3., 4. };
  EXPECT_DOUBLE_EQ(mhd_reynolds_magnetique(u, 2, 0.1, 2.), 0.25);
}

TEST(MHDPosttraitement, MagneticPrandtl)
{
  EXPECT_DOUBLE_EQ(mhd_prandtl_magnetique(0.5, 0.25), 2.);
}
