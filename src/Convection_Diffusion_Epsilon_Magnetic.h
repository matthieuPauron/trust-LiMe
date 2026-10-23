/****************************************************************************
 * trust-LiMe — transport equation for magnetic fluctuation dissipation eps_b.
 *
 * Scaffolding (Phase 3). Full implementation: convection by u, diffusion by
 * eta+eta_t, source C_eps1*(eps_b/k_b)*P_b - C_eps2*eps_b^2/k_b.
 ****************************************************************************/

#ifndef Convection_Diffusion_Epsilon_Magnetic_included
#define Convection_Diffusion_Epsilon_Magnetic_included

#include <Objet_U.h>

class Convection_Diffusion_Epsilon_Magnetic : public Objet_U
{
  Declare_instanciable(Convection_Diffusion_Epsilon_Magnetic);
public:
  // TODO (Phase 3): subclass Convection_Diffusion_Temperature_base.
};

#endif
