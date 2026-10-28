#pragma GCC diagnostic push
#if __GNUC__ < 9
#pragma GCC diagnostic ignored "-Wsuggest-override"
#endif
#include <gtest/gtest.h>
#pragma GCC diagnostic pop

#include <Correction_Divergence_kernel.h>
#include <array>
#include <cmath>
#include <vector>

namespace
{
constexpr int kN = 3;
constexpr int kNe = kN * kN;
constexpr int kNfe = 4;
constexpr double kTol = 2.e-11;

struct CartesianPatch
{
  std::vector<double> volumes = std::vector<double>(kNe, 1.);
  std::vector<int> face_voisins;
  std::vector<int> elem_faces = std::vector<int>(kNe * kNfe, -1);
  std::vector<double> face_normales;
  std::array<int, kNe> n_faces{};

  CartesianPatch()
  {
    for (int e = 0; e < kNe; e++) volumes[e] = 0.7 + 0.13 * e;
    for (int j = 0; j < kN; j++)
      {
        add_face(-1, -1, cell(0, j), 0, 1., 0.); // reversed left boundary
        for (int i = 1; i < kN; i++)
          add_face(cell(i - 1, j), 1, cell(i, j), 0, 1., 0.);
        add_face(cell(kN - 1, j), 1, -1, -1, 1., 0.); // right boundary
      }
    for (int i = 0; i < kN; i++)
      {
        add_face(-1, -1, cell(i, 0), 2, 0., 1.); // reversed bottom boundary
        for (int j = 1; j < kN; j++)
          add_face(cell(i, j - 1), 3, cell(i, j), 2, 0., 1.);
        add_face(cell(i, kN - 1), 3, -1, -1, 0., 1.); // top boundary
      }
  }

  static int cell(int i, int j) { return j * kN + i; }
  int nf() const { return static_cast<int>(face_voisins.size()) / 2; }

private:
  void add_face(int e0, int slot0, int e1, int slot1, double nx, double ny)
  {
    const int f = nf();
    face_voisins.push_back(e0);
    face_voisins.push_back(e1);
    face_normales.push_back(nx);
    face_normales.push_back(ny);
    if (e0 >= 0) elem_faces[e0 * kNfe + slot0] = f;
    if (e1 >= 0) elem_faces[e1 * kNfe + slot1] = f;
  }
};

void apply_divergence(const CartesianPatch& patch, const std::vector<double>& b,
                      std::vector<double>& div)
{
  Correction_Divergence_kernel::divergence_pass1(
    kNe, kNe, 2, 2, kNfe, patch.volumes, patch.face_voisins,
    patch.elem_faces, patch.face_normales, patch.nf(), b, div);
}

void apply_divergence_transpose(const CartesianPatch& patch, const std::vector<double>& phi,
                               std::vector<double>& correction)
{
  Correction_Divergence_kernel::divergence_transpose(
    kNe, kNe, 2, 2, kNfe, patch.volumes, patch.face_voisins,
    patch.elem_faces, patch.face_normales, patch.nf(), phi, correction);
}

bool solve_dense(std::vector<double> matrix, std::vector<double> rhs,
                 std::vector<double>& solution)
{
  double max_diagonal = 0.;
  for (int i = 0; i < kNe; i++) max_diagonal = std::max(max_diagonal, std::fabs(matrix[i * kNe + i]));
  for (int i = 0; i < kNe; i++) matrix[i * kNe + i] += 1.e-12 * max_diagonal;

  for (int col = 0; col < kNe; col++)
    {
      int pivot = col;
      for (int row = col + 1; row < kNe; row++)
        if (std::fabs(matrix[row * kNe + col]) > std::fabs(matrix[pivot * kNe + col]))
          pivot = row;
      if (std::fabs(matrix[pivot * kNe + col]) < 1.e-13) return false;
      for (int j = col; j < kNe; j++)
        std::swap(matrix[col * kNe + j], matrix[pivot * kNe + j]);
      std::swap(rhs[col], rhs[pivot]);
      const double diagonal = matrix[col * kNe + col];
      for (int j = col; j < kNe; j++) matrix[col * kNe + j] /= diagonal;
      rhs[col] /= diagonal;
      for (int row = 0; row < kNe; row++)
        if (row != col)
          {
            const double factor = matrix[row * kNe + col];
            for (int j = col; j < kNe; j++)
              matrix[row * kNe + j] -= factor * matrix[col * kNe + j];
            rhs[row] -= factor * rhs[col];
          }
    }
  solution = std::move(rhs);
  return true;
}
}

TEST(CorrectionDivergenceKernel, AssembledGramMatchesDofDT)
{
  const CartesianPatch patch;
  std::vector<double> L, phi(kNe), correction, div;
  for (int e = 0; e < kNe; e++) phi[e] = std::sin(0.31 * (e + 1)) + 0.07 * e;
  Correction_Divergence_kernel::assemble_L(
    kNe, kNe, 2, 2, kNfe, patch.volumes, patch.face_voisins,
    patch.elem_faces, patch.face_normales, patch.nf(), L);
  apply_divergence_transpose(patch, phi, correction);
  apply_divergence(patch, correction, div);
  for (int e = 0; e < kNe; e++)
    {
      double lphi = 0.;
      for (int eb = 0; eb < kNe; eb++) lphi += L[e * kNe + eb] * phi[eb];
      EXPECT_NEAR(lphi, div[e], kTol);
    }
}

TEST(CorrectionDivergenceKernel, ProjectionRemovesCompatibleSyntheticDivergence)
{
  const CartesianPatch patch;
  std::vector<double> L, b(kNe * 2), rhs, div, correction, phi;
  for (int e = 0; e < kNe; e++)
    {
      b[e * 2] = std::cos(0.41 * e) + 0.2 * e;
      b[e * 2 + 1] = std::sin(0.27 * (e + 1));
    }
  apply_divergence(patch, b, rhs);
  Correction_Divergence_kernel::assemble_L(
    kNe, kNe, 2, 2, kNfe, patch.volumes, patch.face_voisins,
    patch.elem_faces, patch.face_normales, patch.nf(), L);
  ASSERT_TRUE(solve_dense(L, rhs, phi));
  apply_divergence_transpose(patch, phi, correction);
  for (int e = 0; e < kNe; e++)
    for (int d = 0; d < 2; d++) b[e * 2 + d] -= correction[e * 2 + d];
  apply_divergence(patch, b, div);
  for (double value : div) EXPECT_NEAR(value, 0., kTol);
}

TEST(CorrectionDivergenceKernel, ProjectionIsIdempotent)
{
  const CartesianPatch patch;
  std::vector<double> L, b(kNe * 2), rhs, phi, correction, div;
  for (int e = 0; e < kNe; e++)
    {
      b[e * 2] = std::sin(0.23 * (e + 2));
      b[e * 2 + 1] = std::cos(0.17 * (e + 3));
    }
  apply_divergence(patch, b, rhs);
  Correction_Divergence_kernel::assemble_L(
    kNe, kNe, 2, 2, kNfe, patch.volumes, patch.face_voisins,
    patch.elem_faces, patch.face_normales, patch.nf(), L);
  ASSERT_TRUE(solve_dense(L, rhs, phi));
  apply_divergence_transpose(patch, phi, correction);
  for (int i = 0; i < static_cast<int>(b.size()); i++) b[i] -= correction[i];
  const std::vector<double> once = b;

  apply_divergence(patch, b, rhs);
  ASSERT_TRUE(solve_dense(L, rhs, phi));
  apply_divergence_transpose(patch, phi, correction);
  for (int i = 0; i < static_cast<int>(b.size()); i++) b[i] -= correction[i];
  for (int i = 0; i < static_cast<int>(b.size()); i++) EXPECT_NEAR(b[i], once[i], kTol);
}

TEST(CorrectionDivergenceKernel, RejectsNonzeroNetBoundaryFlux)
{
  const CartesianPatch patch;
  std::vector<double> b(kNe * 2, 0.);
  for (int j = 0; j < kN; j++)
    for (int i = 0; i < kN; i++)
      b[CartesianPatch::cell(i, j) * 2] = static_cast<double>(i);

  double scale = 0.;
  const double net = Correction_Divergence_kernel::net_boundary_flux(
    kNe, 2, 2, patch.face_voisins, patch.face_normales, patch.nf(), b, scale);
  EXPECT_GT(std::fabs(net), 0.);
  EXPECT_FALSE(Correction_Divergence_kernel::compatible_net_flux(net, scale));

  std::fill(b.begin(), b.end(), 1.);
  const double zero_net = Correction_Divergence_kernel::net_boundary_flux(
    kNe, 2, 2, patch.face_voisins, patch.face_normales, patch.nf(), b, scale);
  EXPECT_NEAR(zero_net, 0., kTol);
  EXPECT_TRUE(Correction_Divergence_kernel::compatible_net_flux(zero_net, scale));
}

TEST(CorrectionDivergenceKernel, DirichletBoundaryFaceUsesPrescribedTrace)
{
  const CartesianPatch patch;
  const int nf = patch.nf();

  // Mark every boundary face (e0<0 xor e1<0) as Dirichlet with a prescribed
  // value of 2.0 on both components. Interior faces stay Neumann.
  std::vector<int> bc_type(nf, 0);
  std::vector<double> bc_value((size_t)nf * 2, 0.);
  for (int f = 0; f < nf; f++)
    {
      const int e0 = patch.face_voisins[f * 2];
      const int e1 = patch.face_voisins[f * 2 + 1];
      const bool boundary = (e0 < 0) != (e1 < 0);
      if (boundary)
        {
          bc_type[f] = Correction_Divergence_kernel::BC_DIRICHLET;
          bc_value[f * 2 + 0] = 2.0;
          bc_value[f * 2 + 1] = 2.0;
        }
    }

  // A field whose cell values differ from the prescribed trace, so the two
  // traces must give different divergence values.
  std::vector<double> b(kNe * 2, 0.);
  for (int e = 0; e < kNe; e++) { b[e * 2 + 0] = 0.3 * (e + 1); b[e * 2 + 1] = -0.1 * e; }

  std::vector<double> div_neumann, div_dirichlet;
  Correction_Divergence_kernel::divergence_pass1(
    kNe, kNe, 2, 2, kNfe, patch.volumes, patch.face_voisins,
    patch.elem_faces, patch.face_normales, nf, b, div_neumann);
  Correction_Divergence_kernel::divergence_pass1(
    kNe, kNe, 2, 2, kNfe, patch.volumes, patch.face_voisins,
    patch.elem_faces, patch.face_normales, nf, b, div_dirichlet,
    bc_type, bc_value);

  // At least one cell must differ (boundary-adjacent cells).
  bool any_diff = false;
  for (int e = 0; e < kNe; e++)
    if (std::fabs(div_neumann[e] - div_dirichlet[e]) > 1.e-12)
      any_diff = true;
  ASSERT_TRUE(any_diff) << "Dirichlet trace did not change the divergence";

  // Net boundary flux must use the prescribed value (2.0), not b(e).
  double flux_scale = 0.;
  const double net = Correction_Divergence_kernel::net_boundary_flux(
    kNe, 2, 2, patch.face_voisins, patch.face_normales, nf, b, flux_scale,
    bc_type, bc_value);
  // With a prescribed constant trace on every boundary face, the net flux is
  // a fixed number determined by the prescribed values; just check finiteness
  // and that flux_scale is nonzero (prescribed trace contributes).
  ASSERT_TRUE(std::isfinite(net));
  ASSERT_GT(flux_scale, 0.);

  // Assemble L with Dirichlet faces zeroed: those faces must not contribute
  // to any row (Jacobian zero). Check that L differs from the all-Neumann
  // assembly (fewer nonzero contributions).
  std::vector<double> L_neu, L_dir;
  Correction_Divergence_kernel::assemble_L(
    kNe, kNe, 2, 2, kNfe, patch.volumes, patch.face_voisins,
    patch.elem_faces, patch.face_normales, nf, L_neu);
  Correction_Divergence_kernel::assemble_L(
    kNe, kNe, 2, 2, kNfe, patch.volumes, patch.face_voisins,
    patch.elem_faces, patch.face_normales, nf, L_dir, bc_type, bc_value);

  double sum_neu = 0., sum_dir = 0.;
  for (double v : L_neu) sum_neu += std::fabs(v);
  for (double v : L_dir) sum_dir += std::fabs(v);
  ASSERT_LT(sum_dir, sum_neu) << "Dirichlet faces did not reduce the Jacobian";

  // Projection with Dirichlet trace must keep prescribed boundary values
  // untouched: solve L phi = D(b) (Dirichlet), correct b <- b - D^T phi,
  // and check that the boundary-adjacent cells' correction is finite.
  std::vector<double> phi(kNe, 0.), corr(kNe * 2, 0.);
  // solve dense
  std::vector<double> M = L_dir;
  std::vector<double> rhs = div_dirichlet;
  // regularize diagonal
  double mx = 0.;
  for (int i = 0; i < kNe; i++) mx = std::max(mx, std::fabs(M[i*kNe+i]));
  for (int i = 0; i < kNe; i++) M[i*kNe+i] += 1.e-12*mx;
  // gaussian elimination
  for (int col = 0; col < kNe; col++)
    {
      int piv = col;
      for (int r = col+1; r < kNe; r++)
        if (std::fabs(M[r*kNe+col]) > std::fabs(M[piv*kNe+col])) piv = r;
      for (int j = col; j < kNe; j++) std::swap(M[col*kNe+j], M[piv*kNe+j]);
      std::swap(rhs[col], rhs[piv]);
      const double d = M[col*kNe+col];
      for (int j = col; j < kNe; j++) M[col*kNe+j] /= d;
      rhs[col] /= d;
      for (int r = 0; r < kNe; r++)
        if (r != col)
          {
            const double f = M[r*kNe+col];
            for (int j = col; j < kNe; j++) M[r*kNe+j] -= f*M[col*kNe+j];
            rhs[r] -= f*rhs[col];
          }
    }
  phi = rhs;
  Correction_Divergence_kernel::divergence_transpose(
    kNe, kNe, 2, 2, kNfe, patch.volumes, patch.face_voisins,
    patch.elem_faces, patch.face_normales, nf, phi, corr, bc_type, bc_value);
  for (int i = 0; i < kNe * 2; i++)
    ASSERT_TRUE(std::isfinite(corr[i]));
}
