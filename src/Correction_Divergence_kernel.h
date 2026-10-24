#ifndef Correction_Divergence_kernel_included
#define Correction_Divergence_kernel_included

#include <vector>
#include <utility>
#include <cmath>

/*! @brief Framework-free kernels for the Gauss-law Hodge projection.
 *
 *  Pure functions operating on plain connectivity tables (no mesh, no TRUST
 *  object). They define the Pass-1 divergence and its exact transpose, used
 *  by the TRUST equation and arith-only tests. Interior faces use centred
 *  averages; boundary faces use the adjacent element value.
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

using DivergenceRow = std::vector<std::pair<int, double>>;
using DivergenceRows = std::vector<DivergenceRow>;

// Sparse rows of the exact Pass-1 divergence operator, including ghost rows
// needed by the transpose on partition interfaces. Columns are flattened
// (element, magnetic component) indices.
inline void assemble_divergence_rows(int ne_tot, int nb_b, int dim, int nfe,
                                     const std::vector<double>& volumes,
                                     const std::vector<int>& face_voisins_flat,
                                     const std::vector<int>& elem_faces_flat,
                                     const std::vector<double>& face_normales_flat,
                                     DivergenceRows& rows)
{
  rows.assign(ne_tot, {});
  for (int e = 0; e < ne_tot; e++)
    {
      const double inv_ve = 1. / volumes[e];
      for (int j = 0; j < nfe; j++)
        {
          const int f = elem_faces_flat[e * nfe + j];
          if (f < 0) continue;
          const bool is_e0 = (face_voisins_flat[f * 2] == e);
          const int other = is_e0 ? face_voisins_flat[f * 2 + 1] : face_voisins_flat[f * 2];
          const double sign = is_e0 ? 1. : -1.;
          const double self_weight = other < 0 ? 1. : 0.5;
          for (int c = 0; c < nb_b && c < dim; c++)
            {
              const double coeff = sign * face_normales_flat[f * dim + c] * inv_ve;
              rows[e].emplace_back(e * nb_b + c, self_weight * coeff);
              if (other >= 0 && other < ne_tot)
                rows[e].emplace_back(other * nb_b + c, 0.5 * coeff);
            }
        }
    }
}

inline void transpose_columns(const DivergenceRows& rows, int nb_dofs,
                              std::vector<DivergenceRow>& columns)
{
  columns.assign(nb_dofs, {});
  for (int r = 0; r < static_cast<int>(rows.size()); r++)
    for (const auto& [dof, coeff] : rows[r])
      if (dof >= 0 && dof < nb_dofs)
        columns[dof].emplace_back(r, coeff);
}

// L = D . D^T, assembled densely row-major for conversion to Matrice_Morse.
// Using the exact transpose avoids the checkerboard null modes of composing
// two cell-centred Green-Gauss reconstructions.
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
  DivergenceRows rows;
  assemble_divergence_rows(ne_tot, nb_b, dim, nfe, volumes, face_voisins_flat,
                           elem_faces_flat, face_normales_flat, rows);
  std::vector<DivergenceRow> columns;
  transpose_columns(rows, ne_tot * nb_b, columns);
  for (int e = 0; e < ne; e++)
    {
      for (const auto& [dof, coeff_e] : rows[e])
        for (const auto& [r, coeff_r] : columns[dof])
          L[e * ne_tot + r] += coeff_e * coeff_r;
    }
}

// Apply D^T to a scalar potential. `phi` includes ghost rows; only the first
// `ne` (owned) magnetic field cells are needed by the caller.
inline void divergence_transpose(int ne, int ne_tot, int nb_b, int dim, int nfe,
                                 const std::vector<double>& volumes,
                                 const std::vector<int>& face_voisins_flat,
                                 const std::vector<int>& elem_faces_flat,
                                 const std::vector<double>& face_normales_flat,
                                 int nf, const std::vector<double>& phi,
                                 std::vector<double>& out)
{
  (void)nf;
  DivergenceRows rows;
  assemble_divergence_rows(ne_tot, nb_b, dim, nfe, volumes, face_voisins_flat,
                           elem_faces_flat, face_normales_flat, rows);
  std::vector<DivergenceRow> columns;
  transpose_columns(rows, ne_tot * nb_b, columns);
  out.assign((size_t)ne_tot * nb_b, 0.);
  for (int e = 0; e < ne; e++)
    for (int c = 0; c < nb_b; c++)
      {
        const int dof = e * nb_b + c;
        for (const auto& [r, coeff] : columns[dof])
          out[dof] += coeff * phi[r];
      }
}

} // namespace Correction_Divergence_kernel

#endif
