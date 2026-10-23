/****************************************************************************
 * trust-LiMe — turbulent magnetic diffusivity from a k, epsilon field.
 *
 * Champ_Fonc_Eta_Turbulent evaluates, element by element,
 *
 *     eta_t(x) = C_mu * k(x)^2 / eps(x) / Pm_t
 *
 * from two operand fields (k, eps) and two scalars (C_mu, Pm_t). It is the
 * Phase 2b producer for the induction-turbulence effective diffusivity: the
 * result is handed to `diffusivite_turbulente_magnetique` (or used directly)
 * and then folded into eta_eff by Champ_Fonc_Somme (Phase 2a).
 *
 * Operands are observed (OBS_PTR) so a k/eps that is itself a Champ_Fonc
 * (e.g. from a hydraulic turbulence model) is tracked automatically.
 * A floor is applied to eps to avoid division by zero.
 ****************************************************************************/

#ifndef Champ_Fonc_Eta_Turbulent_included
#define Champ_Fonc_Eta_Turbulent_included

#include <Champ_Fonc_Elem_PolyMAC_CDO.h>
#include <TRUST_Ref.h>

class Champ_base;
class Convection_Diffusion_Magnetic_field;

class Champ_Fonc_Eta_Turbulent : public Champ_Fonc_Elem_PolyMAC_CDO
{
  Declare_instanciable(Champ_Fonc_Eta_Turbulent);

public:
  /*! @brief Binds the field to its domain, operands and constants. */
  void configurer(const Convection_Diffusion_Magnetic_field& eqn,
                  const Champ_base& k, int k_comp,
                  const Champ_base& eps, int eps_comp,
                  double C_mu, double Pm_t, bool realizable);

  /*! @brief Recomputes eta_t, then forwards time bookkeeping. */
  void mettre_a_jour(double tps) override;

private:
  void calculer();

  OBS_PTR(Convection_Diffusion_Magnetic_field) eqn_;
  OBS_PTR(Champ_base) k_;
  OBS_PTR(Champ_base) eps_;
  int k_comp_;
  int eps_comp_;
  double C_mu_;
  double Pm_t_;
  bool realizable_;
};

#endif /* Champ_Fonc_Eta_Turbulent_included */
