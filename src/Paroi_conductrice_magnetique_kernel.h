/****************************************************************************
 * trust-LiMe — header-only helper for conducting-wall boundary conditions.
 *
 * For a perfectly conducting wall, the tangential components of b are clamped
 * to Dirichlet 0 while the normal component keeps a zero flux (Neumann).  This
 * helper returns, for a face normal n, the index of the magnetic component
 * aligned with the normal (argmax |n_d|) when the field is a full vector
 * (nb_composantes == dimension).  For a reduced (scalar) model
 * (nb_composantes < dimension) every component is tangential and the helper
 * returns -1.
 *
 * The function is intentionally free of any TRUST framework dependency beyond
 * the array type, so it can be unit-tested directly.
 ****************************************************************************/

#ifndef PAROICONDUCTRICEMAGNETIQUEKERNEL_INCLUDED
#define PAROICONDUCTRICEMAGNETIQUEKERNEL_INCLUDED

#include <TRUSTTab.h>

#include <cmath>
#include <cstddef>
#include <vector>

/*! @brief Index of the component aligned with the normal (argmax |n_d|).
 *
 *  Returns -1 when nb_composantes < dimension (scalar model: all tangential)
 *  or when the inputs are invalid.  Ties are broken towards the lowest index.
 */
inline int normal_component_index(const double* n, int dim, int nb_composantes)
{
  if (n == nullptr || dim <= 0 || nb_composantes <= 0)
    return -1;
  if (nb_composantes < dim)
    return -1; // scalar model: every component is tangential
  if (nb_composantes != dim)
    return -1;

  int idx = 0;
  double best = std::fabs(n[0]);
  for (int d = 1; d < dim; d++)
    {
      const double v = std::fabs(n[d]);
      if (v > best)
        {
          best = v;
          idx = d;
        }
    }
  return idx;
}

inline int normal_component_index(const std::vector<double>& n, int nb_composantes)
{
  if (n.empty())
    return -1;
  return normal_component_index(n.data(), static_cast<int>(n.size()), nb_composantes);
}

inline int normal_component_index(const DoubleVect& n, int nb_composantes)
{
  const int dim = static_cast<int>(n.size());
  if (dim <= 0)
    return -1;
  std::vector<double> v(static_cast<std::size_t>(dim));
  for (int d = 0; d < dim; d++)
    v[static_cast<std::size_t>(d)] = n(d);
  return normal_component_index(v.data(), dim, nb_composantes);
}

inline int normal_component_index(const DoubleTab& n, int nb_composantes)
{
  int dim = 0;
  std::vector<double> v;
  if (n.nb_dim() == 1)
    {
      dim = n.dimension(0);
      v.resize(static_cast<std::size_t>(dim));
      for (int d = 0; d < dim; d++)
        v[static_cast<std::size_t>(d)] = n(d);
    }
  else if (n.dimension(0) == 1)
    {
      dim = n.dimension(1);
      v.resize(static_cast<std::size_t>(dim));
      for (int d = 0; d < dim; d++)
        v[static_cast<std::size_t>(d)] = n(0, d);
    }
  else if (n.dimension(1) == 1)
    {
      dim = n.dimension(0);
      v.resize(static_cast<std::size_t>(dim));
      for (int d = 0; d < dim; d++)
        v[static_cast<std::size_t>(d)] = n(d, 0);
    }
  else
    {
      dim = n.dimension(1);
      v.resize(static_cast<std::size_t>(dim));
      for (int d = 0; d < dim; d++)
        v[static_cast<std::size_t>(d)] = n(0, d);
    }
  if (dim <= 0)
    return -1;
  return normal_component_index(v.data(), dim, nb_composantes);
}

template <class V>
inline int normal_component_index(const V& n, int nb_composantes)
{
  const int dim = static_cast<int>(n.size());
  if (dim <= 0)
    return -1;
  std::vector<double> v(static_cast<std::size_t>(dim));
  for (int d = 0; d < dim; d++)
    v[static_cast<std::size_t>(d)] = n[d];
  return normal_component_index(v.data(), dim, nb_composantes);
}

#endif
