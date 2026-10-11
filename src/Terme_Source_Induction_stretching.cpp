/****************************************************************************
 * trust-LiMe — M1 induction source term: S = (b . grad) u
 * See Terme_Source_Induction_stretching.h for the model description.
 ****************************************************************************/

#include <Terme_Source_Induction_stretching.h>
#include <Induction_stretching_kernel.h>
#include <Convection_Diffusion_Magnetic_field.h>
#include <Discret_Thyd.h>
#include <Domaine_dis_base.h>
#include <Domaine_Cl_dis_base.h>
#include <Champ_Inc_base.h>
#include <Champ_Fonc_base.h>
#include <Probleme_base.h>
#include <Matrice_Morse.h>
#include <Param.h>
#include <Process.h>

Implemente_instanciable(Terme_Source_Induction_stretching,
                        "induction_stretching",
                        Source_base);
// XD induction_stretching source_base induction_stretching BRACE
// XD_CONT Induction source term S = (b.grad) u added to the right-hand side of the standalone magnetic
// XD_CONT equation. It is the source that appears when the induction equation is written in
// XD_CONT convection-diffusion form for an incompressible fluid. The velocity u is the (given, frozen)
// XD_CONT transporting velocity of the equation, so grad u is constant and the term is explicit in b.

Sortie& Terme_Source_Induction_stretching::printOn(Sortie& os) const
{
  return os << que_suis_je() << finl;
}

Entree& Terme_Source_Induction_stretching::readOn(Entree& is)
{
  // The source carries no data of its own (the velocity comes from the
  // equation), but Sources::readOn expects every source to consume its own
  // braces before looking for the next ',' or '}'. Accept an empty block
  // "induction_stretching { }".
  Param param(que_suis_je());
  set_param(param);
  param.lire_avec_accolades_depuis(is);
  return is;
}

void Terme_Source_Induction_stretching::set_param(Param& param) const
{
  // No data-file keyword for now; the velocity is read by the equation.
}

/*! @brief Retrieves the magnetic equation that owns this source.
 */
void Terme_Source_Induction_stretching::associer_pb(const Probleme_base& pb)
{
  const int nb_eq = pb.nombre_d_equations();
  for (int i = 0; i < nb_eq; i++)
    {
      const Equation_base& eqn = pb.equation(i);
      if (sub_type(Convection_Diffusion_Magnetic_field, eqn))
        equation_mag_ = ref_cast(Convection_Diffusion_Magnetic_field, eqn);
    }
  if (!equation_mag_)
    {
      Cerr << "Error: the 'induction_stretching' source term requires a "
           "Convection_Diffusion_Magnetic_field equation in problem "
           << pb.que_suis_je() << finl;
      Process::exit();
    }
  dim_ = Objet_U::dimension;
  nb_compos_b_ = equation_mag_->nb_composantes();
}

/*! @brief Stores the discretised domain (unused here, kept for the framework).
 */
void Terme_Source_Induction_stretching::associer_domaines(const Domaine_dis_base&, const Domaine_Cl_dis_base&)
{
  // Nothing to store: grad u is built at initialisation from the equation.
}

/*! @brief Builds the constant velocity gradient grad(u) and records the
 *         number of transported magnetic components.
 *
 *  Because the velocity is frozen (given data), grad(u) is computed once here
 *  with the kernel field `gradient_vitesse` (Discret_Thyd::grad_u). This is the
 *  same grad(u) the momentum equation would use, so the coupling is consistent.
 */
int Terme_Source_Induction_stretching::initialiser(double temps)
{
  const Convection_Diffusion_Magnetic_field& eqn = equation_mag_.valeur();

  // grad(u): face velocity -> cell-centred grad with dim*dim components.
  const Discret_Thyd& dis = ref_cast(Discret_Thyd, eqn.discretisation());
  dis.grad_u(eqn.domaine_dis(), eqn.domaine_Cl_dis(), eqn.vitesse_transportante(), gradient_vitesse_);
  gradient_vitesse_->mettre_a_jour(temps);

  dim_ = Objet_U::dimension;
  nb_compos_b_ = eqn.nb_composantes();

  Cerr << "Initialisation of the (b.grad)u induction source term"
       << " (grad u: " << dim_ * dim_ << " components," << nb_compos_b_ << " magnetic components)"
       << finl;
  return 1;
}

/*! @brief Adds S = (b.grad) u to `secmem`.
 *
 *  Explicit (lagged) in b: S_i = sum_j  b_j * (d u_i / d x_j).
 *  With the kernel gradient field layout (2D: dUdX,dUdY,dVdX,dVdY), the
 *  component (i*dim + j) of grad(u) is d u_i / d x_j.
 */
void Terme_Source_Induction_stretching::ajouter_source(DoubleTab& secmem) const
{
  const DoubleTab& b = equation_mag_.valeur().inconnue().valeurs();  // (nb_elem, nb_compos_b)
  const DoubleTab& gu = gradient_vitesse_->valeurs();               // (nb_elem, dim*dim)

  // The contraction itself lives in a header-only, unit-testable function: see
  // Induction_stretching_kernel.h.
  induction_stretching_ajouter(b, gu, nb_compos_b_, dim_, secmem);
}

DoubleTab& Terme_Source_Induction_stretching::ajouter(DoubleTab& secmem) const
{
  ajouter_source(secmem);
  return secmem;
}

DoubleTab& Terme_Source_Induction_stretching::calculer(DoubleTab& secmem) const
{
  secmem = 0.;
  return ajouter(secmem);
}

/*! @brief Bloc interface: this source has no matrix contribution (explicit),
 *         it only adds to the right-hand side.
 */
void Terme_Source_Induction_stretching::dimensionner_blocs(matrices_t, const tabs_t&) const
{
  // Explicit source: contributes nothing to the Jacobian.
}

void Terme_Source_Induction_stretching::ajouter_blocs(matrices_t, DoubleTab& secmem, const tabs_t&) const
{
  ajouter_source(secmem);
}