/****************************************************************************
 * trust-LiMe — point-wise sum of two fields.
 *
 * Champ_Fonc_Somme is a P0 element field whose value at every element is the
 * sum of two source fields (typically the molecular and the turbulent magnetic
 * diffusivities). It lets the induction equation's effective diffusivity
 * eta_eff = eta + eta_t be represented as a single field even when eta_t is
 * spatially varying (Phase 2 of the induction-turbulence plan), instead of
 * being restricted to the uniform case folded in readOn().
 *
 * The two operands are stored as OBS_PTR(Champ_base). Each mettre_a_jour()
 * recomputes the values from the current state of the operands, so a
 * turbulent diffusivity that is itself a Champ_Fonc (e.g. derived from k, eps)
 * is tracked automatically.
 ****************************************************************************/

#ifndef Champ_Fonc_Somme_included
#define Champ_Fonc_Somme_included

#include <Champ_Fonc_Elem_PolyMAC_CDO.h>
#include <TRUST_Ref.h>

class Champ_base;
class Convection_Diffusion_Magnetic_field;

class Champ_Fonc_Somme : public Champ_Fonc_Elem_PolyMAC_CDO
{
  Declare_instanciable(Champ_Fonc_Somme);

public:
  /*! @brief Binds the field to its domain and the two operands. */
  void configurer(const Convection_Diffusion_Magnetic_field& eqn,
                  const Champ_base& a, const Champ_base& b);

  /*! @brief Recomputes the sum, then forwards time bookkeeping. */
  void mettre_a_jour(double tps) override;

private:
  void calculer();

  OBS_PTR(Champ_base) a_;
  OBS_PTR(Champ_base) b_;
};

#endif /* Champ_Fonc_Somme_included */
