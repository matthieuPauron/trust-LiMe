/****************************************************************************
 * trust-LiMe — turbulent magnetic diffusivity from a k, epsilon field.
 ****************************************************************************/

#include <Champ_Fonc_Eta_Turbulent.h>
#include <Convection_Diffusion_Magnetic_field.h>
#include <Champ_base.h>
#include <Process.h>
#include <TRUSTTab.h>
#include <cmath>

Implemente_instanciable(Champ_Fonc_Eta_Turbulent,
                        "Champ_Fonc_Eta_Turbulent",
                        Champ_Fonc_Elem_PolyMAC_CDO);
// XD Champ_Fonc_Eta_Turbulent champ_don_base Champ_Fonc_Eta_Turbulent BRACE Turbulent magnetic diffusivity eta_t = C_mu*k^2/eps/Pm_t.

void Champ_Fonc_Eta_Turbulent::configurer(const Convection_Diffusion_Magnetic_field& eqn,
                                          const Champ_base& k, int k_comp,
                                          const Champ_base& eps, int eps_comp,
                                          double C_mu, double Pm_t, bool realizable,
                                          double epsilon_min, double eta_max)
{
  eqn_ = eqn;
  k_ = k;
  eps_ = eps;
  k_comp_ = k_comp;
  eps_comp_ = eps_comp;
  C_mu_ = C_mu;
  Pm_t_ = (Pm_t > 0.0) ? Pm_t : 1.0;
  realizable_ = realizable;
  epsilon_min_ = epsilon_min;
  eta_max_ = eta_max;
  if (epsilon_min_ <= 0. || eta_max_ <= 0.)
    Process::exit("Champ_Fonc_Eta_Turbulent: epsilon_min and eta_max must be positive.");

  nommer("eta_turbulent_magnetique");
  associer_domaine_dis_base(eqn.domaine_dis());

  fixer_nature_du_champ(Nature_du_champ::scalaire);
  fixer_nb_comp(1);

  fixer_nb_valeurs_nodales(le_dom_VF->nb_elem());
}

void Champ_Fonc_Eta_Turbulent::mettre_a_jour(double tps)
{
  // Refresh operands first (they may be Champ_Fonc themselves).
  if (k_)
    {
      Champ_Fonc_base* cf = dynamic_cast<Champ_Fonc_base*>(&k_.valeur());
      if (cf) cf->mettre_a_jour(tps);
    }
  if (eps_)
    {
      Champ_Fonc_base* cf = dynamic_cast<Champ_Fonc_base*>(&eps_.valeur());
      if (cf) cf->mettre_a_jour(tps);
    }
  calculer();
  Champ_Fonc_Elem_PolyMAC_CDO::mettre_a_jour(tps);
}

void Champ_Fonc_Eta_Turbulent::calculer()
{
  if (!k_ || !eps_)
    return;

  DoubleTab& ve = valeurs();
  const int ne = ve.dimension(0);
  const int nc = ve.dimension(1);

  const DoubleTab& vk = k_.valeur().valeurs();
  const DoubleTab& ve_ = eps_.valeur().valeurs();

  // Velocity gradient (only needed for the realizable closure).
  DoubleTab g;
  if (realizable_ && eqn_)
    eqn_->gradient_vitesse_elements(g);

  const int dim = Objet_U::dimension;
  for (int e = 0; e < ne; e++)
    for (int c = 0; c < nc; c++)
      {
        double kk = (vk.dimension(0) == 1) ? vk(0, k_comp_) : vk(e, k_comp_);
        double ee = (ve_.dimension(0) == 1) ? ve_(0, eps_comp_) : ve_(e, eps_comp_);
        if (ee < epsilon_min_) ee = epsilon_min_;
        if (kk < 0.) kk = 0.;

        double Cmu = C_mu_;
        if (realizable_ && g.size() > 0)
          {
            // Strain rate S = sqrt(2 S_ij S_ij), vorticity W = sqrt(2 W_ij W_ij).
            double S2 = 0., W2 = 0.;
            for (int i = 0; i < dim; i++)
              for (int j = 0; j < dim; j++)
                {
                  const double dudx = g(e, i * dim + j);
                  const double dudx_t = g(e, j * dim + i);
                  const double Sij = 0.5 * (dudx + dudx_t);
                  const double Wij = 0.5 * (dudx - dudx_t);
                  S2 += 2. * Sij * Sij;
                  W2 += 2. * Wij * Wij;
                }
            const double S = std::sqrt(S2);
            const double W = std::sqrt(W2);
            // Shih realizable closure.
            const double Sk_eps = (ee > 0.) ? (S * kk / ee) : 0.;
            const double arg = (Sk_eps > 0.) ? (std::sqrt(6.) * W * kk / (S * ee)) : 0.;
            const double phi = (1. / 3.) * std::acos(std::min(1., std::max(-1., arg)));
            const double As = std::sqrt(6.) * std::cos(phi);
            const double denom = 4.04 + As * Sk_eps;
            Cmu = (denom > 0.) ? (1. / denom) : C_mu_;
            if (Cmu < 1e-4) Cmu = 1e-4;
            if (Cmu > 0.5) Cmu = 0.5;
          }
        ve(e, c) = std::min(Cmu * kk * kk / ee / Pm_t_, eta_max_);
      }
}

Sortie& Champ_Fonc_Eta_Turbulent::printOn(Sortie& os) const
{
  return Champ_Fonc_Elem_PolyMAC_CDO::printOn(os);
}

Entree& Champ_Fonc_Eta_Turbulent::readOn(Entree& is)
{
  return Champ_Fonc_Elem_PolyMAC_CDO::readOn(is);
}
