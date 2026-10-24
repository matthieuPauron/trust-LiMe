/****************************************************************************
 * trust-LiMe — scalar transport equation for magnetic-fluctuation energy k_b.
 ****************************************************************************/

#ifndef Convection_Diffusion_K_Magnetic_included
#define Convection_Diffusion_K_Magnetic_included

#include <Convection_Diffusion_Temperature_base.h>
#include <TRUST_Ref.h>

class Modele_turbulence_magnetique;

class Convection_Diffusion_K_Magnetic : public Convection_Diffusion_Temperature_base
{
  Declare_instanciable(Convection_Diffusion_K_Magnetic);

public:
  void discretiser() override;
  void associer_milieu_base(const Milieu_base& mil) override;
  void completer() override;
  void mettre_a_jour(double temps) override;
  const Champ_Inc_base& inconnue() const override { return k_b_; }
  Champ_Inc_base& inconnue() override { return k_b_; }
  const Champ_Don_base& diffusivite_pour_transport() const override;
  const Champ_base& diffusivite_pour_pas_de_temps() const override;
  const Champ_base& vitesse_pour_transport() const override;
  void associer_modele(const Modele_turbulence_magnetique& modele) { modele_ = modele; }
  const Modele_turbulence_magnetique& modele() const { return modele_.valeur(); }

private:
  OWN_PTR(Champ_Inc_base) k_b_;
  OBS_PTR(Modele_turbulence_magnetique) modele_;
};

#endif
