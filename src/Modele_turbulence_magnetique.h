/****************************************************************************
 * trust-LiMe — independent magnetic-fluctuation k-epsilon closure.
 ****************************************************************************/

#ifndef Modele_turbulence_magnetique_included
#define Modele_turbulence_magnetique_included

#include <Objet_U.h>
#include <TRUST_Ref.h>
#include <TRUST_Deriv.h>

class Champ_Don_base;
class Champ_Fonc_base;
class Champ_Inc_base;
class Convection_Diffusion_K_Magnetic;
class Convection_Diffusion_Epsilon_Magnetic;
class Convection_Diffusion_Magnetic_field;

/*! @brief Independent two-equation closure for magnetic fluctuations.
 *
 * k_b and epsilon_b are solved by the two scalar equations registered in the
 * hosting problem's optional-equation list. This object owns the closure
 * coefficients/fields and binds those equations to the magnetic equation.
 */
class Modele_turbulence_magnetique : public Objet_U
{
  Declare_instanciable(Modele_turbulence_magnetique);

public:
  void set_param(Param& param) const override;
  void completer(const Convection_Diffusion_Magnetic_field& eqn);
  void mettre_a_jour(double temps);
  void controler_et_mettre_a_jour(double temps);
  void calculer_production(DoubleTab& production) const;

  const Champ_Don_base& eta_t() const;
  const Champ_Don_base& diffusivite_k() const;
  const Champ_Don_base& diffusivite_epsilon() const;
  const Convection_Diffusion_K_Magnetic& equation_k() const;
  const Convection_Diffusion_Epsilon_Magnetic& equation_epsilon() const;
  double k_min() const { return k_min_; }
  double epsilon_min() const { return epsilon_min_; }
  double C_eps1() const { return C_eps1_; }
  double C_eps2() const { return C_eps2_; }
  double rho_ref() const { return rho_ref_; }

private:
  void limiter(Champ_Inc_base& champ, int& count) const;

  OBS_PTR(Convection_Diffusion_Magnetic_field) equation_magnetique_;
  OBS_PTR(Convection_Diffusion_K_Magnetic) equation_k_;
  OBS_PTR(Convection_Diffusion_Epsilon_Magnetic) equation_epsilon_;

  OWN_PTR(Champ_Fonc_base) eta_t_;
  OWN_PTR(Champ_Fonc_base) eta_t_k_;
  OWN_PTR(Champ_Fonc_base) eta_t_epsilon_;
  OWN_PTR(Champ_Fonc_base) diffusivite_k_;
  OWN_PTR(Champ_Fonc_base) diffusivite_epsilon_;

  double C_mu_b_ = 0.09;
  double C_eps1_ = 1.44;
  double C_eps2_ = 1.92;
  double sigma_kb_ = 1.0;
  double sigma_epsilonb_ = 1.3;
  double Pm_t_b_ = 1.0;
  double k_min_ = 1.e-20;
  double epsilon_min_ = 1.e-12;
  double eta_t_max_ = 1.e30;
  double rho_ref_ = 0.;
  double field_scale_ = 0.;
};

#endif
