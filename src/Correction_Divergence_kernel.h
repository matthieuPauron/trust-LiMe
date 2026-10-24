#ifndef Correction_Divergence_kernel_included
#define Correction_Divergence_kernel_included

#include <vector>
#include <cmath>

/*! @brief Framework-free kernels for the Gauss-law Hodge projection.
 *
 *  Pure functions operating on plain connectivity tables (no mesh, no TRUST
 *  object). They are the single source of truth shared by the TRUST class
 *  `Correction_Divergence` (which wraps the assembled matrix into a
 *  `Matrice_Morse`) and the arith-only unit tests (which call them on
 *  hand-built 3x3 patches). Every index convention mirrors
 *  `Convection_Diffusion_Magnetic_field::gradient_b_elements` (Pass 1):
 *  interior faces use the centred average of the two adjacent element values;
 *  boundary faces use the element's own value (zero normal gradient).
 */

namespace Correction_Divergence_kernel
{

inline bool compatible_net_flux(double net_flux, double flux_scale)
{
  return std::fabs(net_flux) <= 1.e-12 * (1. + flux_scale);
}

// Outward magnetic flux on physical boundary faces, using the same boundary
// trace as divergence_pass1 (the adjacent element value).
inline double net_boundary_flux(int ne_tot, int nb_b, int dim,
                                const std::vector<int>& face_voisins_flat,
                                const std::vector<double>& face_normales_flat,
                                int nf, const std::vector<double>& b,
                                double& flux_scale)
{
  double net = 0.;
  flux_scale = 0.;
  for (int f = 0; f < nf; f++)
    {
      const int e0 = face_voisins_flat[f * 2], e1 = face_voisins_flat[f * 2 + 1];
      if ((e0 < 0) == (e1 < 0)) continue;
      const int e = e0 >= 0 ? e0 : e1;
      if (e >= ne_tot) continue;
      const double orientation = e0 >= 0 ? 1. : -1.;
      for (int c = 0; c < nb_b && c < dim; c++)
        {
          const double flux = orientation * b[e * nb_b + c] * face_normales_flat[f * dim + c];
          net += flux;
          flux_scale += std::fabs(flux);
        }
    }
  return net;
}

// D(b)(e) = (1/V_e) sum_{f in d(e)} sgn_ef * (b_f . N_f)
// with b_f = centred average (interior) or b(e) (wall).
inline void divergence_pass1(int ne, int ne_tot, int nb_b, int dim, int nfe,
                             const std::vector<double>& volumes,
                             const std::vector<int>& face_voisins_flat, // size nf*2
                             const std::vector<int>& elem_faces_flat,   // size ne*nfe
                             const std::vector<double>& face_normales_flat, // size nf*dim
                             int nf,
                             const std::vector<double>& b, // size ne_tot*nb_b
                             std::vector<double>& out)      // size ne
{
  (void)ne_tot;
  (void)nf;
  out.assign(ne, 0.);
  for (int e = 0; e < ne; e++)
    {
      const double inv_ve = 1. / volumes[e];
      for (int j = 0; j < nfe; j++)
        {
          const int f = elem_faces_flat[e * nfe + j];
          if (f < 0) continue;
          const bool is_e0 = (face_voisins_flat[f * 2 + 0] == e);
          const int e_other = is_e0 ? face_voisins_flat[f * 2 + 1] : face_voisins_flat[f * 2 + 0];
          const double sgn = is_e0 ? 1. : -1.;
          for (int c = 0; c < nb_b && c < dim; c++)
            {
              const double bf = (e_other < 0) ? b[e * nb_b + c] : 0.5 * (b[e * nb_b + c] + b[e_other * nb_b + c]);
              out[e] += sgn * bf * face_normales_flat[f * dim + c] * inv_ve;
            }
        }
    }
}

// G(phi)(e,d) = (1/V_e) sum_{f in d(e)} sgn_ef * phi_f * N_f,d
// with phi_f = centred average (interior) or phi(e) (wall).
inline void gradient_pass1(int ne, int ne_tot, int dim, int nfe,
                           const std::vector<double>& volumes,
                           const std::vector<int>& face_voisins_flat,
                           const std::vector<int>& elem_faces_flat,
                           const std::vector<double>& face_normales_flat,
                           int nf,
                           const std::vector<double>& phi, // size ne_tot
                           std::vector<double>& out)        // size ne*dim, row-major
{
  (void)nf;
  out.assign((size_t)ne * dim, 0.);
  for (int e = 0; e < ne; e++)
    {
      const double inv_ve = 1. / volumes[e];
      for (int j = 0; j < nfe; j++)
        {
          const int f = elem_faces_flat[e * nfe + j];
          if (f < 0) continue;
          const bool is_e0 = (face_voisins_flat[f * 2 + 0] == e);
          const int e_other = is_e0 ? face_voisins_flat[f * 2 + 1] : face_voisins_flat[f * 2 + 0];
          const double sgn = is_e0 ? 1. : -1.;
          const double phif = (e_other < 0) ? phi[e] : 0.5 * (phi[e] + phi[e_other]);
          for (int d = 0; d < dim; d++)
            out[e * dim + d] += sgn * phif * face_normales_flat[f * dim + d] * inv_ve;
        }
    }
}

// L = D . G assembled into a dense ne x ne_tot matrix (row-major).
// L(e, eb) += sgn_ef * w_ec * (1/V_ec) * sgn2 * w' * (N_f . N_f')
// for every chain f (face of e) -> ec (cell of f) -> f' (face of ec) -> eb (cell of f').
inline void assemble_L(int ne, int ne_tot, int nb_b, int dim, int nfe,
                       const std::vector<double>& volumes,
                       const std::vector<int>& face_voisins_flat,
                       const std::vector<int>& elem_faces_flat,
                       const std::vector<double>& face_normales_flat,
                       int nf,
                       std::vector<double>& L) // size ne*ne_tot, zeroed by caller
{
  (void)nf;
  L.assign((size_t)ne * ne_tot, 0.);
  for (int e = 0; e < ne; e++)
    {
      for (int j = 0; j < nfe; j++)
        {
          const int f = elem_faces_flat[e * nfe + j];
          if (f < 0) continue;
          const bool is_e0 = (face_voisins_flat[f * 2 + 0] == e);
          const int e0f = face_voisins_flat[f * 2 + 0];
          const int e1f = face_voisins_flat[f * 2 + 1];
          const double sgn_ef = is_e0 ? 1. : -1.;
          // cells of face f and their weights
          // build list (ec, weight)
          int ecs[2];
          double ws[2];
          int n_ec = 0;
          if (e0f >= 0) { ecs[n_ec] = e0f; ws[n_ec] = (e1f >= 0) ? 0.5 : 1.0; n_ec++; }
          if (e1f >= 0) { ecs[n_ec] = e1f; ws[n_ec] = 0.5; n_ec++; }
          for (int a = 0; a < n_ec; a++)
            {
              const int ec = ecs[a];
              const double w_ec = ws[a];
              if (ec < 0 || ec >= ne_tot) continue;
              const double inv_ve = 1. / volumes[ec];
              for (int j2 = 0; j2 < nfe; j2++)
                {
                  const int fp = elem_faces_flat[ec * nfe + j2];
                  if (fp < 0) continue;
                  const bool is_ec0 = (face_voisins_flat[fp * 2 + 0] == ec);
                  const int e0p = face_voisins_flat[fp * 2 + 0];
                  const int e1p = face_voisins_flat[fp * 2 + 1];
                  const double sgn2 = is_ec0 ? 1. : -1.;
                  int ebs[2];
                  double wps[2];
                  int n_eb = 0;
                  if (e0p >= 0) { ebs[n_eb] = e0p; wps[n_eb] = (e1p >= 0) ? 0.5 : 1.0; n_eb++; }
                  if (e1p >= 0) { ebs[n_eb] = e1p; wps[n_eb] = 0.5; n_eb++; }
                  for (int b = 0; b < n_eb; b++)
                    {
                      const int eb = ebs[b];
                      const double wp = wps[b];
                      if (eb < 0 || eb >= ne_tot) continue;
                      double ndot = 0.;
                       for (int d = 0; d < nb_b && d < dim; d++)
                         ndot += face_normales_flat[f * dim + d] * face_normales_flat[fp * dim + d];
                      const double coeff = sgn_ef * w_ec * inv_ve * sgn2 * wp * ndot;
                      L[e * ne_tot + eb] += coeff;
                    }
                }
            }
        }
    }
}

} // namespace Correction_Divergence_kernel

#endif
