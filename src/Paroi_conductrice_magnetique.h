/****************************************************************************
 * trust-LiMe — perfect conductor (homogeneous Dirichlet on tangential
 * components) magnetic boundary condition.
 ****************************************************************************/
#ifndef Paroi_conductrice_magnetique_included
#define Paroi_conductrice_magnetique_included

#include <Cond_lim_base.h>

/*! @brief `paroi_conductrice_magnetique`: perfectly conducting magnetic wall.
 *
 *  Tangential components of b are clamped to Dirichlet 0, the normal
 *  component keeps a zero flux (Neumann).  Registered under the magnetic
 *  keyword with app-domains Magnetique / Thermique / indetermine.
 */
class Paroi_conductrice_magnetique : public Cond_lim_base
{
  Declare_instanciable(Paroi_conductrice_magnetique);
};

#endif
