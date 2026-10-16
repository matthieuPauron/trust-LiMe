/****************************************************************************
 * trust-LiMe — magnetic isolation (homogeneous Neumann) boundary condition. *
 ****************************************************************************/
#ifndef Paroi_isolante_magnetique_included
#define Paroi_isolante_magnetique_included

#include <Neumann_paroi_adiabatique.h>

/*! @brief `paroi_isolante_magnetique`: perfectly insulating magnetic wall.
 *
 *  Homogeneous Neumann (zero normal flux of b) on every component.  Alias of
 *  `Neumann_Paroi_adiabatique`, with magnetic_eq compatible app_domains.
 */
class Paroi_isolante_magnetique : public Neumann_paroi_adiabatique
{
  Declare_instanciable(Paroi_isolante_magnetique);
};
#endif
