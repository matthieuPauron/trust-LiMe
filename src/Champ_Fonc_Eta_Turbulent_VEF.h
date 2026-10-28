#ifndef Champ_Fonc_Eta_Turbulent_VEF_included
#define Champ_Fonc_Eta_Turbulent_VEF_included

#include <Champ_Fonc_P0_VEF.h>
#include <TRUST_Ref.h>

class Champ_base;
class Convection_Diffusion_Magnetic_field;

class Champ_Fonc_Eta_Turbulent_VEF : public Champ_Fonc_P0_VEF
{
  Declare_instanciable(Champ_Fonc_Eta_Turbulent_VEF);

public:
  void configurer(const Convection_Diffusion_Magnetic_field& eqn,
                  const Champ_base& k, int k_comp,
                  const Champ_base& eps, int eps_comp,
                  double C_mu, double Pm_t, bool realizable,
                  double epsilon_min = 1.e-12, double eta_max = 1.e30);
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
  double epsilon_min_ = 1.e-12;
  double eta_max_ = 1.e30;
};

#endif
