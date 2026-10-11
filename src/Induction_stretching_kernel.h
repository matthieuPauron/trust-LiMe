/****************************************************************************
 * trust-LiMe — pure arithmetic kernel of the M1 source term S = (b . grad) u.
 *
 * This header is deliberately free of any TRUST framework dependency beyond
 * the array type: it contains the *business logic* of the inductive stretching
 * term, i.e. the index convention that maps the kernel velocity-gradient field
 * onto the transported magnetic components. That convention is the single most
 * error-prone part of the source term (a transposed layout still "works" for a
 * diagonal gradient), which is exactly why it lives in a separate, directly
 * unit-testable function.
 *
 * The 2D layout of the kernel field `gradient_vitesse` is
 *
 *      component 0 = d u_x / d x      component 1 = d u_x / d y
 *      component 2 = d u_y / d x      component 3 = d u_y / d y
 *
 * i.e. component (i * dim + j) is d u_i / d x_j. See
 * PolyMAC_CDO_discretisation::grad_u() and grad_U_Champ_Face_PolyMAC_CDO.
 ****************************************************************************/

#ifndef INDUCTION_STRETCHING_KERNEL_INCLUDED
#define INDUCTION_STRETCHING_KERNEL_INCLUDED

#include <TRUSTTab.h>   // DoubleTab = TRUSTTab<double, int> (full definition)
#include <cassert>

/*! @brief Adds S = (b.grad) u to `secmem`: S_i = sum_j b_j * (d u_i / d x_j).
 *
 *  @param b           the magnetic field, shaped (nb_elem, nb_compos_b).
 *  @param grad_u      the kernel velocity gradient, shaped
 *                     (nb_elem, dim*dim), component (i*dim+j) = d u_i/d x_j.
 *  @param nb_compos_b number of transported magnetic components (1..dim).
 *  @param dim         space dimension (2 in this phase).
 *  @param secmem      the right-hand side, shaped (nb_elem, nb_comp_src);
 *                     the source is *accumulated* into it.
 *
 *  The contraction is deliberately looped over j < nb_compos_b only: when the
 *  user transports fewer magnetic components than space dimensions
 *  (`nb_composantes 1` in 2D, for instance), the untransported directions must
 *  not contribute.
 */
inline void induction_stretching_ajouter(const DoubleTab& b,
                                         const DoubleTab& grad_u,
                                         const int nb_compos_b,
                                         const int dim,
                                         DoubleTab& secmem)
{
  assert(nb_compos_b >= 1 && nb_compos_b <= dim);
  assert(b.dimension(0) == secmem.dimension(0));
  assert(grad_u.dimension(0) == secmem.dimension(0));
  assert(grad_u.dimension(1) >= dim * dim);

  const int nb_elem = b.dimension(0);
  const int nb_comp_src = secmem.dimension(1);

  for (int ie = 0; ie < nb_elem; ie++)
    for (int i = 0; i < nb_comp_src; i++)
      {
        double s = 0.;
        for (int j = 0; j < nb_compos_b; j++)
          s += b(ie, j) * grad_u(ie, i * dim + j);
        secmem(ie, i) += s;
      }
}

#endif /* INDUCTION_STRETCHING_KERNEL_INCLUDED */