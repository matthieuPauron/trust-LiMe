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

/*---------------------------------------------------------------------------
 * Magnetic tension Ft = (b . grad) b / mu0 — layout g[i*dim+j] = d b_i / d x_j.
 *--------------------------------------------------------------------------*/
TEST(MHDPosttraitement, TensionIdentityGradientGivesBOverMu0)
{
  // identity grad: d b_i/dx_j = delta_ij -> Ft_i = b_i / mu0
  const double b[2] = { 1., 2. };
  const double g[4] = { 1., 0., 0., 1. };   // [[dbx/dx, dbx/dy], [dby/dx, dby/dy]]
  double ft[2] = { 0., 0. };
  mhd_tension_magnetique(b, g, 2, 2, 1., ft);
  EXPECT_DOUBLE_EQ(ft[0], 1.);
  EXPECT_DOUBLE_EQ(ft[1], 2.);

  mhd_tension_magnetique(b, g, 2, 2, 4., ft);
  EXPECT_DOUBLE_EQ(ft[0], 0.25);
  EXPECT_DOUBLE_EQ(ft[1], 0.5);
}

TEST(MHDPosttraitement, TensionZeroGradientIsZero)
{
  const double b[3] = { 5., -1., 2. };
  const double g[9] = { 0. };
  double ft[3] = { 1., 1., 1. };
  mhd_tension_magnetique(b, g, 3, 3, 2., ft);
  for (int d = 0; d < 3; d++)
    EXPECT_DOUBLE_EQ(ft[d], 0.);
}

TEST(MHDPosttraitement, TensionIsIndexCorrect)
{
  // pin the layout: g[0] = db_x/dx, g[1] = db_x/dy, g[2] = db_y/dx, g[3] = db_y/dy
  const double b[2] = { 3., -2. };      // (bx, by)
  const double g[4] = { 10., 1., 0., 0. };
  double ft[2] = { 0., 0. };
  mhd_tension_magnetique(b, g, 2, 2, 1., ft);
  // Ft_x = (1/mu0) (bx * db_x/dx + by * db_x/dy) = 3*10 + (-2)*1 = 28
  // Ft_y = (1/mu0) (bx * db_x/dx[WrongShouldBeDb_y/dx] + by * db_y/dy) = 3*0 + (-2)*0 = 0
  EXPECT_DOUBLE_EQ(ft[0], 28.);
  EXPECT_DOUBLE_EQ(ft[1], 0.);

  // swap directions to prove the index order is structural (not row/col-symmetric)
  const double g2[4] = { 0., 10., 0., 1. };
  mhd_tension_magnetique(b, g2, 2, 2, 1., ft);
  // Ft_x = bx * 0 + by * 10 = -20 ; Ft_y = bx * 0 + by * 1 = -2
  EXPECT_NEAR(ft[0], -20., 1e-14);
  EXPECT_NEAR(ft[1], -2., 1e-14);
}

/*---------------------------------------------------------------------------
 * Curl, divergence, current density and Lorentz force from the element
 * gradient of b (layout g[i*dim+j] = d b_i / d x_j).
 *--------------------------------------------------------------------------*/
TEST(MHDPosttraitement, Curl2DIsTheSkewPart)
{
  // g = [[0, 1], [1, 0]] (b = (y, x)) -> w_z = d by/dx - d bx/dy = 1 - 1 = 0
  const double g[4] = { 0., 1., 1., 0. };
  double w[1] = { 0. };
  mhd_curl_b(g, 2, w);
  EXPECT_DOUBLE_EQ(w[0], 0.);

  // g = [[0, -1], [1, 0]] (b = (-y, x)) -> w_z = 1 - (-1) = 2
  const double g2[4] = { 0., -1., 1., 0. };
  mhd_curl_b(g2, 2, w);
  EXPECT_DOUBLE_EQ(w[0], 2.);
}

TEST(MHDPosttraitement, Curl3DIsTheSkewPart)
{
  // b = (y, z, x) -> grad = [[0,1,0],[0,0,1],[1,0,0]] -> curl = (-1,-1,-1)
  const double g[9] = { 0., 1., 0., 0., 0., 1., 1., 0., 0. };
  double w[3] = { 0., 0., 0. };
  mhd_curl_b(g, 3, w);
  EXPECT_DOUBLE_EQ(w[0], -1.);
  EXPECT_DOUBLE_EQ(w[1], -1.);
  EXPECT_DOUBLE_EQ(w[2], -1.);
}

TEST(MHDPosttraitement, DivergenceIsTheTrace)
{
  // b = (x, y) in 2D -> div b = 1 + 1 = 2
  const double g[4] = { 1., 0., 0., 1. };
  EXPECT_DOUBLE_EQ(mhd_div_b(g, 2, 2), 2.);
  // b = (y, z, x) in 3D -> div b = 0 (off-diagonal gradient)
  const double g3[9] = { 0., 1., 0., 0., 0., 1., 1., 0., 0. };
  EXPECT_DOUBLE_EQ(mhd_div_b(g3, 3, 3), 0.);
}

TEST(MHDPosttraitement, CourantIsCurlOverMu0)
{
  // J = curl(b)/mu0 ; 2D single component
  const double g[4] = { 0., -1., 1., 0. };   // w_z = 2
  double w[1] = { 0. }, j[1] = { 0. };
  mhd_curl_b(g, 2, w);
  mhd_courant_magnetique(w, 2, 4., j);
  EXPECT_DOUBLE_EQ(j[0], 0.5);
  // 3D vector
  const double g3[9] = { 0., 1., 0., 0., 0., 1., 1., 0., 0. };  // w = (-1,-1,-1)
  double w3[3] = { 0., 0., 0. }, j3[3] = { 0., 0., 0. };
  mhd_curl_b(g3, 3, w3);
  mhd_courant_magnetique(w3, 3, 2., j3);
  EXPECT_DOUBLE_EQ(j3[0], -0.5);
  EXPECT_DOUBLE_EQ(j3[1], -0.5);
  EXPECT_DOUBLE_EQ(j3[2], -0.5);
}

TEST(MHDPosttraitement, LorentzForceIsCurlCrossBOverMu0)
{
  // 2D: b = (-y, x), w_z = 2, mu0 = 1 -> F = (-w_z b_y, w_z b_x) = (-2x, -2y)
  const double g[4] = { 0., -1., 1., 0. };
  const double b[2] = { -0.45, 0.55 };   // (bx, by) at the probe
  double w[1] = { 0. }, fl[2] = { 0., 0. };
  mhd_curl_b(g, 2, w);
  mhd_force_lorentz(w, b, 2, 2, 1., fl);
  EXPECT_NEAR(fl[0], -2. * 0.55, 1e-14);   // -w_z * b_y = -2 * 0.55 = -1.1
  EXPECT_NEAR(fl[1], -2. * 0.45, 1e-14);   //  w_z * b_x =  2 * (-0.45) = -0.9

  // 3D: w = (-1,-1,-1), b = (y, z, x) at probe -> b = (0.375, 0.375, 0.375)
  const double g3[9] = { 0., 1., 0., 0., 0., 1., 1., 0., 0. };
  const double b3[3] = { 0.375, 0.375, 0.375 };
  double w3[3] = { 0., 0., 0. }, fl3[3] = { 0., 0., 0. };
  mhd_curl_b(g3, 3, w3);
  mhd_force_lorentz(w3, b3, 3, 3, 1., fl3);
  // (w x b) = (w_y b_z - w_z b_y, w_z b_x - w_x b_z, w_x b_y - w_y b_x) = (0,0,0)
  EXPECT_NEAR(fl3[0], 0., 1e-14);
  EXPECT_NEAR(fl3[1], 0., 1e-14);
  EXPECT_NEAR(fl3[2], 0., 1e-14);
}
