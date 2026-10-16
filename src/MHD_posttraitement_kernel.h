/****************************************************************************
 * trust-LiMe — pure arithmetic kernels of the MHD post-treatment fields.
 *
 * This header is deliberately free of any TRUST framework dependency (not
 * even TRUSTTab): it holds the *physics conventions* of the derived fields
 * registered by Convection_Diffusion_Magnetic_field (see docs/content/model.md
 * "Post-treatment fields"). Working on plain per-element pointers keeps every
 * formula directly unit-testable without a mesh, exactly like
 * Induction_stretching_kernel.h does for the M1 source term.
 *
 * Conventions:
 *   b    : magnetic field of a cell, nb_b components (0 < nb_b <= dim);
 *          untransported components behave as 0 (scalar model = b along x).
 *   u    : velocity of the cell, always dim components.
 *   g    : cell-centred velocity gradient, row-major layout
 *          g[i * dim + j] = du_i / d x_j (grad_Champ_Face_PolyMAC_MPFA layout,
 *          re-extracted per element), phase index dropped (single phase here).
 *   eta  : magnetic diffusivity of the cell.
 *   nu   : kinematic viscosity of the cell.
 *   mu0  : magnetic permeability (module default: 1).
 *   qm   : charge-to-mass ratio q/m (module default: e/m_e, positron charge).
 ****************************************************************************/

#ifndef MHD_POSTTRAITEMENT_KERNEL_INCLUDED
#define MHD_POSTTRAITEMENT_KERNEL_INCLUDED

#include <cassert>
#include <cmath>

/*! @brief |b|^2 of the cell: sum over the nb_b transported components
 *         (untransported components are absent from the table, they act as 0).
 */
inline double mhd_norme2_b(const double* b, const int nb_b)
{
  assert(nb_b >= 1);
  double s = 0.;
  for (int c = 0; c < nb_b; c++)
    s += b[c] * b[c];
  return s;
}

/*! @brief Magnetic energy density e_m = |b|^2 / (2 mu0). */
inline double mhd_energie_magnetique(const double b2, const double mu0)
{
  return b2 / (2. * mu0);
}

/*! @brief Local cyclotron pulsation omega_c = (q/m) * |b|. */
inline double mhd_pulsation_cyclotronique(const double b2, const double qm)
{
  return qm * std::sqrt(b2);
}

/*! @brief Crossed helicity density h_c = u . b  (sum over min(nb_b, dim)). */
inline double mhd_produit_scalar_u_b(const double* u, const double* b, const int nb_b, const int dim)
{
  assert(nb_b >= 1 && nb_b <= dim);
  double s = 0.;
  for (int c = 0; c < nb_b; c++)
    s += u[c] * b[c];
  return s;
}

/*! @brief Ideal-MHD Poynting vector from the frozen velocity u:
 *         S = (1/mu0) ( |b|^2 u - (u.b) b ),     E = -u x b  =>  S = E x b / mu0.
 *         Missing b components (nb_b < dim) act as 0.
 */
inline void mhd_vecteur_poynting(const double* u, const double* b, const int nb_b, const int dim,
                                 const double mu0, double* s)
{
  const double b2 = mhd_norme2_b(b, nb_b);
  const double ub = mhd_produit_scalar_u_b(u, b, nb_b, dim);
  for (int i = 0; i < dim; i++)
    {
      const double bi = (i < nb_b) ? b[i] : 0.;
      s[i] = (b2 * u[i] - ub * bi) / mu0;
    }
}

/*! @brief Vorticity (curl of u) from the element gradient g[i*dim+j] = du_i/dx_j.
 *         2D: w has a single value, the out-of-plane component w_z.
 *         3D: w = (dw?  order (x,y,z)).
 */
inline void mhd_curl_u_elem(const double* g, const int dim, double* w)
{
  if (dim == 2)
    {
      // w_z = du_y/dx - du_x/dy = g[1*2+0] - g[0*2+1]
      w[0] = g[2] - g[1];
    }
  else
    {
      assert(dim == 3);
      w[0] = g[7] - g[5]; // du_z/dy - du_y/dz
      w[1] = g[2] - g[6]; // du_x/dz - du_z/dx
      w[2] = g[3] - g[1]; // du_y/dx - du_x/dy
    }
}

/*! @brief Kinetic helicity density h_k = u . (curl u).
 *         In 2D curl(u) is perpendicular to the plane and u is in-plane,
 *         so h_k = 0 identically.
 */
inline double mhd_helicite_cinetique(const double* u, const double* w, const int dim)
{
  if (dim == 2)
    return 0.;
  assert(dim == 3);
  return u[0] * w[0] + u[1] * w[1] + u[2] * w[2];
}

/*! @brief Local magnetic Reynolds number Rm = |u| * L / eta
 *         (L: local cell size, e.g. V_e^(1/dim), the magnetic twin of the
 *         kernel "Reynolds_maille" = |u| * taille_maille / nu).
 */
inline double mhd_reynolds_magnetique(const double* u, const int dim, const double taille, const double eta)
{
  double u2 = 0.;
  for (int d = 0; d < dim; d++)
    u2 += u[d] * u[d];
  return std::sqrt(u2) * taille / eta;
}

/*! @brief Magnetic Prandtl number Pm = nu / eta. */
inline double mhd_prandtl_magnetique(const double nu, const double eta)
{
  return nu / eta;
}

/*! @brief Magnetic tension force density Ft = (1/mu0) (b . grad) b,
 *         i.e. Ft_i = (1/mu0) sum_j b_j * d b_i / d x_j.
 *
 *  @param b     the magnetic field at the element, nb_b components.
 *  @param grad_b the element gradient of b, row-major layout:
 *                grad_b[i * dim + j] = d b_i / d x_j (same convention as the
 *                grad_u layout used by helicite_cinetique).
 *  @param nb_b  number of transported components; equals dim for a full vector
 *               field (guarded: the post field rejects the scalar model).
 *  @param dim   space dimension (2 or 3).
 *  @param mu0   magnetic permeability.
 *  @param ft    output vector, dim components.
 */
inline void mhd_tension_magnetique(const double* b, const double* grad_b, const int nb_b, const int dim,
                                   const double mu0, double* ft)
{
  assert(nb_b == dim);
  for (int i = 0; i < dim; i++)
    {
      double acc = 0.;
      for (int j = 0; j < nb_b; j++)
        acc += b[j] * grad_b[i * dim + j];
      ft[i] = acc / mu0;
    }
}

/*! @brief Curl of b from the element gradient: w = curl b = grad x b.
 *         2D: single out-of-plane component w_z = d b_y/dx - d b_x/dy.
 *         3D: w = (w_x, w_y, w_z).
 */
inline void mhd_curl_b(const double* g, const int dim, double* w)
{
  if (dim == 2)
    {
      // w_z = d b_y/dx - d b_x/dy = g[1*2+0] - g[0*2+1]
      w[0] = g[2] - g[1];
    }
  else
    {
      assert(dim == 3);
      w[0] = g[7] - g[5]; // d b_z/dy - d b_y/dz
      w[1] = g[2] - g[6]; // d b_x/dz - d b_z/dx
      w[2] = g[3] - g[1]; // d b_y/dx - d b_x/dy
    }
}

/*! @brief Divergence of b: div b = trace(grad b) = sum_i d b_i/dx_i. */
inline double mhd_div_b(const double* g, const int nb_b, const int dim)
{
  double s = 0.;
  for (int i = 0; i < nb_b; i++)
    s += g[i * dim + i];
  return s;
}

/*! @brief Electric current density J = curl(b) / mu0 (Ampere's law).
 *         2D: single out-of-plane component J_z. 3D: vector.
 */
inline void mhd_courant_magnetique(const double* w, const int dim, const double mu0, double* j)
{
  if (dim == 2)
    j[0] = w[0] / mu0;
  else
    for (int d = 0; d < dim; d++)
      j[d] = w[d] / mu0;
}

/*! @brief Lorentz force density F = J x b = (curl b) x b / mu0.
 *         2D: w = (0,0,w_z), b in-plane -> F = (-w_z b_y, w_z b_x)/mu0.
 *         3D: full cross product (w x b)/mu0.
 */
inline void mhd_force_lorentz(const double* w, const double* b, const int nb_b, const int dim,
                              const double mu0, double* fl)
{
  if (dim == 2)
    {
      // (0,0,w_z) x (b_x,b_y,0) = (-w_z b_y, w_z b_x, 0)
      fl[0] = -w[0] * b[1] / mu0;
      fl[1] = w[0] * b[0] / mu0;
    }
  else
    {
      assert(dim == 3);
      fl[0] = (w[1] * b[2] - w[2] * b[1]) / mu0;
      fl[1] = (w[2] * b[0] - w[0] * b[2]) / mu0;
      fl[2] = (w[0] * b[1] - w[1] * b[0]) / mu0;
    }
}

#endif /* MHD_POSTTRAITEMENT_KERNEL_INCLUDED */
