/****************************************************************************
 * trust-LiMe — transport equation for magnetic fluctuation energy k_b.
 *
 * Scaffolding (Phase 3). Full implementation: convection by u, diffusion by
 * eta+eta_t, source P_b = -b_i b_j d_i u_j - eps_b.
 ****************************************************************************/

#ifndef Convection_Diffusion_K_Magnetic_included
#define Convection_Diffusion_K_Magnetic_included

#include <Objet_U.h>

class Convection_Diffusion_K_Magnetic : public Objet_U
{
  Declare_instanciable(Convection_Diffusion_K_Magnetic);
public:
  // TODO (Phase 3): subclass Convection_Diffusion_Temperature_base instead.
  // TODO (Phase 3): readOn, discretiser, inconnue, diffusivite_pour_transport.
  // TODO (Phase 3): source term Terme_Source_K_Magnetic.
};

#endif
