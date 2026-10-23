/****************************************************************************
 * trust-LiMe — turbulence model for the magnetic field (scaffolding).
 *
 * Modele_turbulence_magnetique is the magnetic analogue of TrioCFD's
 * Modele_turbulence_hyd_K_Eps: it holds the magnetic fluctuation energy k_b
 * and its dissipation epsilon_b, and computes the eddy magnetic diffusivity
 * eta_t = C_mu^m * k_b^2 / epsilon_b.
 *
 * STATUS: scaffolding (Phase 3). The transport equations and source terms are
 * not yet implemented; this class only stores the fields and exposes the
 * interface so that the design is concrete and compilable.
 ****************************************************************************/

#ifndef Modele_turbulence_magnetique_included
#define Modele_turbulence_magnetique_included

#include <Objet_U.h>
#include <TRUST_Ref.h>
#include <TRUST_Deriv.h>

class Champ_Fonc_base;

class Modele_turbulence_magnetique : public Objet_U
{
  Declare_instanciable(Modele_turbulence_magnetique);

public:
  void completer() { }

  // TODO (Phase 3): transport equations for k_b, epsilon_b.
  // TODO (Phase 3): calculer_eddy_diffusivity() -> eta_t.
};

#endif
