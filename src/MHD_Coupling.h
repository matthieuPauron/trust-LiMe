/****************************************************************************
 * trust-LiMe — strong (Picard) coupling of the magnetic and momentum
 * equations, shared by the coupled problems Pb_MHD and Pb_MHD_Turbulent.
 ****************************************************************************/

#ifndef MHD_COUPLING_H_
#define MHD_COUPLING_H_

class Probleme_base;

/*! @brief Advances the problem by one time step, with optional Picard strong
 *         coupling.
 *
 *  Default (`couplage_fort` off on the magnetic equation): a single
 *  Gauss-Seidel sweep over the equations, i.e. the historical fully-lagged
 *  staggered scheme b^(s+1) = F(b^s, u^s), u^(s+1) = G(u^s, b^s).
 *
 *  When `couplage_fort` is on, the magnetic <-> momentum sweep is repeated up
 *  to nb_iterations_couplage() times, or until the relative change of both the
 *  magnetic field and the velocity falls below tolerance_couplage().
 *
 *  The problem's own overridden iterateTimeStep() must not re-enter this
 *  function (the base implementation is called explicitly).
 *
 *  @param[in,out] pb        the coupled problem (magnetic + Navier-Stokes).
 *  @param[out]    converged the time-scheme convergence flag.
 *  @return false on a failed step.
 */
bool mhd_iterate_time_step(Probleme_base& pb, bool& converged);

#endif /* MHD_COUPLING_H_ */
