/****************************************************************************
 * trust-LiMe — magnetic isolation (homogeneous Neumann) boundary condition.
 * See Paroi_isolante_magnetique.h for the model notes.
 ****************************************************************************/

#include <Paroi_isolante_magnetique.h>

Implemente_instanciable(Paroi_isolante_magnetique,
                        "paroi_isolante_magnetique",
                        Neumann_paroi_adiabatique);
// XD paroi_isolante_magnetique Neumann_paroi_adiabatique paroi_isolante_magnetique INHERITS_BRACE
// XD_CONT Perfectly insulating magnetic wall: homogeneous Neumann (zero normal
// XD_CONT flux of b) on every component of the magnetic field.

/*! @brief Writes the object identity (standard TRUST I/O). */
Sortie& Paroi_isolante_magnetique::printOn(Sortie& s) const
{
  return s << que_suis_je() << finl;
}

/*! @brief Reads the boundary condition block (no magnetic-specific keywords).
 *
 *  Declares the application domains ("Magnetique", "Thermique",
 *  "indetermine") on first read so that Cond_lim_base::compatible_avec_eqn()
 *  accepts the condition on both the magnetic and the thermal equations.
 */
Entree& Paroi_isolante_magnetique::readOn(Entree& s)
{
  if (app_domains.size() == 0)
    app_domains = { Motcle("Magnetique"), Motcle("Thermique"),
                    Motcle("indetermine")
                  };
  return Neumann_paroi_adiabatique::readOn(s);
}
