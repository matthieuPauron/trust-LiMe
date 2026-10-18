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
#include <Domaine_VF.h>
#include <Milieu_base.h>
#include <Champ_Inc_base.h>
#include <Champ_Fonc_base.h>
#include <Probleme_base.h>
#include <Matrice_Morse.h>
#include <Param.h>
#include <Process.h>
#include <Champ_Face_PolyMAC_MPFA.h>

Implemente_instanciable(Terme_Source_Induction_stretching,
                        "induction_stretching|induction_stretching_Elem_PolyMAC_MPFA",
                        Source_base);
// XD induction_stretching source_base induction_stretching BRACE
// XD_CONT Induction source term S = (b.grad) u added to the right-hand side of the standalone magnetic
// XD_CONT equation. It is the source that appears when the induction equation is written in
// XD_CONT convection-diffusion form for an incompressible fluid. The velocity u is the (given, frozen)
// XD_CONT transporting velocity of the equation, so grad u is constant and the term is explicit in b.

/*! @brief Writes the object identity (standard TRUST I/O). */
Sortie& Terme_Source_Induction_stretching::printOn(Sortie& os) const
{
  return os << que_suis_je() << finl;
}

/*! @brief Reads the source block, which carries no keyword of its own.
 *
 *  The velocity comes from the equation, but Sources::readOn expects every
 *  source to consume its own braces before looking for the next ',' or '}'.
 *  An empty block "induction_stretching { }" is therefore accepted.
 */
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

/*! @brief Registers the source's keywords: none for now. */
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
  // Two-way mode (equation keyword vitesse_hydraulique): the velocity is the
  // Navier-Stokes unknown and grad(u) must follow it every step.
  vitesse_depend_du_temps_ = equation_mag_->vitesse_hydraulique();
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

  // The kernel gradient of u (Discret_Thyd::grad_u -> grad_Champ_Face_PolyMAC_MPFA)
  // reconstructs the cell-centred grad from the *element* values of the velocity
  // face field, which are auxiliary variables of that field. Operators acting on
  // the velocity normally request them (e.g. Op_Conv_EF_Stab_PolyMAC_MPFA_Face::
  // completer), but here the transported unknown is an Elem field, so its
  // convection/diffusion operators are Elem-based and nobody asks for the
  // velocity's auxiliary variables: they must be requested explicitly, otherwise
  // grad(u) reads out-of-bounds garbage instead of the velocity values.
  const Champ_Inc_base& vit_c = eqn.vitesse_transportante();
  if (sub_type(Champ_Face_PolyMAC_MPFA, vit_c))
    {
      Champ_Face_PolyMAC_MPFA& vit = ref_cast_non_const(Champ_Face_PolyMAC_MPFA, vit_c);
      vit.init_auxiliary_variables();
      // update_ve() gates on the MD_Vector of the *current* time case only; drive
      // it over every temporal case so that passe()/futur() (which grad_u reads)
      // are all populated.
      for (int n = 0; n < vit.nb_valeurs_temporelles(); n++)
        vit.update_ve(vit.futur(n));
    }

  // grad(u): face velocity -> cell-centred grad with dim*dim components.
  const Discret_Thyd& dis = ref_cast(Discret_Thyd, eqn.discretisation());
  dis.grad_u(eqn.domaine_dis(), eqn.domaine_Cl_dis(), eqn.vitesse_transportante(), gradient_vitesse_);
  gradient_vitesse_->mettre_a_jour(temps);

  dim_ = Objet_U::dimension;
  nb_compos_b_ = eqn.nb_composantes();

  // In the two-way mode the velocity changes every step: record the velocity
  // time this build corresponds to, so that maj_grad_u() only rebuilds when
  // the velocity has actually moved on (frozen mode: never).
  temps_grad_u_ = eqn.vitesse_transportante().temps();

  Cerr << "Initialisation of the (b.grad)u induction source term"
       << " (grad u: " << dim_ * dim_ << " components," << nb_compos_b_ << " magnetic components)"
       << finl;
  return 1;
}

/*! @brief Refreshes grad(u) when the transporting velocity is the NS unknown.
 *
 *  The velocity's time changes only at the wheel rotation (once per step),
 *  so the guard makes this one rebuild per time step, even though the source
 *  is evaluated at every implicit iteration.  The rebuild happens at
 *  assembly time (from ajouter_source()), NOT in mettre_a_jour():
 *  Probleme_base::mettre_a_jour updates this equation's sources *before*
 *  the NS inconnue wheel rotates (index order), so a rebuild there would
 *  read a velocity one step older than the convection term, which reads
 *  the live present slot (the PolyMAC evaluator re-references the velocity
 *  values on every flux evaluation).
 */
void Terme_Source_Induction_stretching::maj_grad_u() const
{
  if (!vitesse_depend_du_temps_ || !gradient_vitesse_)
    return;
  const Convection_Diffusion_Magnetic_field& eqn = equation_mag_.valeur();
  const Champ_Inc_base& vit = eqn.vitesse_transportante();
  if (vit.temps() == temps_grad_u_)
    return;

  // Refresh the velocity's element values (auxiliary rows of the face field)
  // so the gradient operator reads the current u.  Idempotent, same recipe
  // as initialiser() -- see KERNEL_REUSE_NOTES.md "Getting a velocity
  // gradient for free".
  if (sub_type(Champ_Face_PolyMAC_MPFA, vit))
    {
      Champ_Face_PolyMAC_MPFA& vit_m = ref_cast_non_const(Champ_Face_PolyMAC_MPFA, vit);
      for (int n = 0; n < vit_m.nb_valeurs_temporelles(); n++)
        vit_m.update_ve(vit_m.futur(n));
    }
  gradient_vitesse_->mettre_a_jour(vit.temps());
  temps_grad_u_ = vit.temps();
}

/*! @brief Advances the cached grad(u) field's time.
 *
 *  The transporting velocity is frozen (given data), so grad u is constant and
 *  no value needs recomputing. The field is still advanced in time because the
 *  framework requires every source term owning a field to overload
 *  Source_base::mettre_a_jour() — the base class exits otherwise.
 */
void Terme_Source_Induction_stretching::mettre_a_jour(double temps)
{
  // Frozen data velocity: keep the historical behaviour (constant field,
  // only its time advances).  Two-way mode: nothing to do here; grad(u) is
  // refreshed lazily at assembly time by maj_grad_u(), so that the convection
  // term and the stretching term always use the same velocity time level.
  if (gradient_vitesse_ && !vitesse_depend_du_temps_)
    gradient_vitesse_->mettre_a_jour(temps);
}

/*! @brief Resets the cached grad(u) field's time (see mettre_a_jour()). */
void Terme_Source_Induction_stretching::resetTime(double temps)
{
  if (gradient_vitesse_)
    gradient_vitesse_->resetTime(temps);
}

/*! @brief Adds S = (b.grad) u to `secmem`.
 *
 *  Explicit (lagged) in b: S_i = sum_j  b_j * (d u_i / d x_j).
 *
 *  The kernel velocity-gradient field (grad_Champ_Face_PolyMAC_MPFA) stores the
 *  face gradients in its first nb_faces_tot() rows, then the element gradients:
 *  row `nb_faces_tot + e*dim + j`, column `i` holds d u_i / d x_j for element e.
 *  We re-extract it into a plain (nb_elem, dim*dim) table (slot i*dim+j =
 *  du_i/dx_j), which is the contract of the unit-testable kernel
 *  induction_stretching_ajouter().
 */
void Terme_Source_Induction_stretching::ajouter_source(DoubleTab& secmem) const
{
  // Two-way mode: refresh grad(u) first, in case the velocity has moved on
  // since the last assembly (no-op for a frozen data velocity).
  maj_grad_u();

  const DoubleTab& b = equation_mag_.valeur().inconnue().valeurs();  // (nb_elem, nb_compos_b)
  const DoubleTab& gu = gradient_vitesse_->valeurs();               // (nf_tot + ne*dim, comp)

  // Per-element gradient, laid out as the kernel expects: g(e, i*dim+j) = du_i/dx_j.
  const Domaine_VF& dvf = ref_cast(Domaine_VF, equation_mag_->domaine_dis());
  const int ne = dvf.nb_elem();
  const int nf_tot = dvf.nb_faces_tot();
  const int D = dim_;
  DoubleTab g(ne, D * D);
  for (int e = 0; e < ne; e++)
    for (int i = 0; i < D; i++)
      for (int j = 0; j < D; j++)
        g(e, i * D + j) = gu(nf_tot + e * D + j, i);

  // Physical rate S_i = sum_j b_j * du_i/dx_j. The contraction itself lives in
  // a header-only, unit-testable function: see Induction_stretching_kernel.h.
  DoubleTrav s(secmem);
  s = 0.;
  induction_stretching_ajouter(b, g, nb_compos_b_, D, s);

  // secmem is the *integrated* right-hand side of the scheme: a volumetric
  // source must be weighted by the cell measure (porosity * volume) before being
  // accumulated — this is the convention used by the kernel element sources
  // (e.g. Terme_Source_Decroissance_Radioactive_Elem_PolyMAC_CDO). Without the
  // weight the source is effectively divided by the cell volume in the update,
  // so the explicit integration of db/dt = b explodes (residual ~1e35).
  const DoubleVect& poro = equation_mag_->milieu().porosite_elem();
  const DoubleVect& vol = dvf.volumes();
  const int nb_elem = secmem.dimension(0), nb_comp = secmem.dimension(1);
  for (int e = 0; e < nb_elem; e++)
    for (int i = 0; i < nb_comp; i++)
      secmem(e, i) += poro(e) * vol(e) * s(e, i);
}

/*! @brief Framework entry point: calls ajouter_source() and returns secmem. */
DoubleTab& Terme_Source_Induction_stretching::ajouter(DoubleTab& secmem) const
{
  ajouter_source(secmem);
  return secmem;
}

/*! @brief Framework entry point: zeroes secmem then adds the source. */
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

/*! @brief Bloc interface: adds S = (b.grad) u to the right-hand side
 *         (same as ajouter(); no matrix contribution).
 */
void Terme_Source_Induction_stretching::ajouter_blocs(matrices_t, DoubleTab& secmem, const tabs_t&) const
{
  ajouter_source(secmem);
}