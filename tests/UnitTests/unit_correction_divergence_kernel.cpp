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
constexpr double kTol = 2.e-13;

struct CartesianPatch
{
  std::vector<double> volumes = std::vector<double>(kNe, 1.);
  std::vector<int> face_voisins;
  std::vector<int> elem_faces = std::vector<int>(kNe * kNfe, -1);
  std::vector<double> face_normales;
  std::array<int, kNe> n_faces{};

  CartesianPatch()
  {
    for (int j = 0; j < kN; j++)
      {
        add_face(cell(0, j), 0, -1, -1, -1., 0.); // left boundary
        for (int i = 1; i < kN; i++)
          add_face(cell(i - 1, j), 1, cell(i, j), 0, 1., 0.);
        add_face(cell(kN - 1, j), 1, -1, -1, 1., 0.); // right boundary
      }
    for (int i = 0; i < kN; i++)
      {
        add_face(cell(i, 0), 2, -1, -1, 0., -1.); // bottom boundary
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
    elem_faces[e0 * kNfe + slot0] = f;
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

void apply_gradient(const CartesianPatch& patch, const std::vector<double>& phi,
                    std::vector<double>& grad)
{
  Correction_Divergence_kernel::gradient_pass1(
    kNe, kNe, 2, kNfe, patch.volumes, patch.face_voisins,
    patch.elem_faces, patch.face_normales, patch.nf(), phi, grad);
}

bool solve_pinned(std::vector<double> matrix, std::vector<double> rhs,
                  std::vector<double>& solution)
{
  // Gauge row phi[0]=0 for the constant null mode.
  for (int j = 0; j < kNe; j++) matrix[j] = 0.;
  matrix[0] = 1.;
  rhs[0] = 0.;

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

TEST(CorrectionDivergenceKernel, AssembledOperatorMatchesDOfG)
{
  const CartesianPatch patch;
  std::vector<double> L, phi(kNe), grad, div;
  for (int e = 0; e < kNe; e++) phi[e] = std::sin(0.31 * (e + 1)) + 0.07 * e;
  Correction_Divergence_kernel::assemble_L(
    kNe, kNe, 2, 2, kNfe, patch.volumes, patch.face_voisins,
    patch.elem_faces, patch.face_normales, patch.nf(), L);
  apply_gradient(patch, phi, grad);
  apply_divergence(patch, grad, div);
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
  std::vector<double> L, potential(kNe), b, rhs, div, correction, phi;
  for (int e = 0; e < kNe; e++) potential[e] = std::cos(0.41 * e) + 0.2 * e;
  apply_gradient(patch, potential, b);
  apply_divergence(patch, b, rhs);
  Correction_Divergence_kernel::assemble_L(
    kNe, kNe, 2, 2, kNfe, patch.volumes, patch.face_voisins,
    patch.elem_faces, patch.face_normales, patch.nf(), L);
  ASSERT_TRUE(solve_pinned(L, rhs, phi));
  apply_gradient(patch, phi, correction);
  for (int e = 0; e < kNe; e++)
    for (int d = 0; d < 2; d++) b[e * 2 + d] -= correction[e * 2 + d];
  apply_divergence(patch, b, div);
  for (double value : div) EXPECT_NEAR(value, 0., kTol);
}

TEST(CorrectionDivergenceKernel, ProjectionIsIdempotent)
{
  const CartesianPatch patch;
  std::vector<double> L, potential(kNe), b, rhs, phi, grad, div;
  for (int e = 0; e < kNe; e++) potential[e] = std::sin(0.23 * (e + 2));
  apply_gradient(patch, potential, b);
  apply_divergence(patch, b, rhs);
  Correction_Divergence_kernel::assemble_L(
    kNe, kNe, 2, 2, kNfe, patch.volumes, patch.face_voisins,
    patch.elem_faces, patch.face_normales, patch.nf(), L);
  ASSERT_TRUE(solve_pinned(L, rhs, phi));
  apply_gradient(patch, phi, grad);
  for (int i = 0; i < static_cast<int>(b.size()); i++) b[i] -= grad[i];
  const std::vector<double> once = b;

  apply_divergence(patch, b, rhs);
  ASSERT_TRUE(solve_pinned(L, rhs, phi));
  apply_gradient(patch, phi, grad);
  for (int i = 0; i < static_cast<int>(b.size()); i++) b[i] -= grad[i];
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
