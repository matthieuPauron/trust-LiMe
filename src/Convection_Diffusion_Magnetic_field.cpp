/****************************************************************************
 * trust-LiMe — magnetic (induction) equation, standalone.
 * See Convection_Diffusion_Magnetic_field.h for the design notes.
 ****************************************************************************/

#include <Convection_Diffusion_Magnetic_field.h>
#include <Probleme_base.h>
#include <Discret_Thyd.h>
#include <Param.h>
#include <Process.h>
#include <Champ_Inc_base.h>
#include <Champ_Don_base.h>

Implemente_instanciable(Convection_Diffusion_Magnetic_field,
                        "Convection_Diffusion_Magnetic_field",
                        Convection_Diffusion_Temperature_base);
// XD Convection_Diffusion_Magnetic_field eqn_base Convection_Diffusion_Magnetic_field INHERITS_BRACE
// XD_CONT Standalone convection-diffusion (induction) equation for the magnetic field b:
// XD_CONT d b / d t + (u . grad) b - div(eta grad b) = 0.
// XD attr diffusivite_magnetique field_base diffusivite_magnetique REQ Magnetic diffusivity eta = 1/(mu0 sigma).
// XD attr vitesse_magnetique field_base vitesse_magnetique OPT Transporting velocity u (default: zero).
// XD attr nb_composantes entier nb_composantes OPT Number of transported components of the magnetic field (default: the
// XD_CONT space dimension). Use 1 for a single magnetic component.

Sortie& Convection_Diffusion_Magnetic_field::printOn(Sortie& os) const
{
  return Convection_Diffusion_Temperature_base::printOn(os);
}

/*! @brief Reads the magnetic equation data block.
 *
 *  Reuses the kernel reading machinery: after the inherited read (which reads
 *  diffusion/convection, sources, boundary and initial conditions), the
 *  transporting velocity is filled from the optional data field, and the mass
 *  solver coefficient is renamed.
 */
Entree& Convection_Diffusion_Magnetic_field::readOn(Entree& is)
{
  assert(the_magnetic_field_);
  Convection_Diffusion_Temperature_base::readOn(is);

  // The convection operator was associated with la_vitesse_transportante_
  // (a Champ_Inc) while the block was read; fill it now from the data field.
  if (vitesse_magnetique_)
    la_vitesse_transportante_->affecter(vitesse_magnetique_.valeur());

  // The magnetic equation is a pure evolution equation: d b / d t.  Its time
  // term must therefore use the identity mass matrix, i.e. no coefficient
  // field.  In particular, the previous value "magnetic_field" was the
  // *unknown's own name*, which made Solveur_Masse_base::appliquer() divide the
  // mass term by b: the scheme became nonlinear and the measured diffusion
  // rate diverged from eta (and from the analytic exp(-eta k^2 t)).
  if (solveur_masse)
    solveur_masse->set_name_of_coefficient_temporel("no_coeff");

  return is;
}

/*! @brief Registers the magnetic keywords on top of the inherited ones. */
void Convection_Diffusion_Magnetic_field::set_param(Param& param) const
{
  Convection_Diffusion_Temperature_base::set_param(param);
  // NOTE: `diffusivite_magnetique` must appear before `diffusion` in the data
  // file (it is queried when the diffusion operator is read).
  param.ajouter("diffusivite_magnetique", &diffusivite_magnetique_);
  param.ajouter("vitesse_magnetique", &vitesse_magnetique_);
  // NOTE: `nb_composantes` is NOT an equation keyword: it must be known before
  // the equations are discretised, so it is read at the *problem* level by
  // Pb_MHD::readOn() (ahead of the medium) and pushed here via
  // set_nb_composantes().
}

/*! @brief Builds the unknown and the transporting velocity fields.
 *
 *  Current scope: up to `dimension` transported components
 *  ("magnetic_field"), discretised with the concentration (multi-scalar
 *  concentration) machinery of Discret_Thyd.
 */
void Convection_Diffusion_Magnetic_field::discretiser()
{
  const Discret_Thyd& dis = ref_cast(Discret_Thyd, discretisation());
  Cerr << "Magnetic field equation discretization" << finl;

  // Nb_composantes must be known before discretisation (read at the problem
  // level by Pb_MHD::readOn()); default: one component per space direction.
  if (nb_composantes_ < 0)
    nb_composantes_ = dimension;
  if (nb_composantes_ < 1 or nb_composantes_ > dimension)
    Process::exit("Convection_Diffusion_Magnetic_field: nb_composantes must be "
                  "between 1 and the space dimension.");
  // Magnetic field: one scalar component.
  dis.concentration(schema_temps(), domaine_dis(), the_magnetic_field_,
                    nb_composantes_, "magnetic_field");
  champs_compris_.ajoute_champ(the_magnetic_field_);

  // Transporting velocity: a vector field, zero unless overridden by data.
  dis.vitesse(schema_temps(), domaine_dis(), la_vitesse_transportante_, 1);
  la_vitesse_transportante_->nommer("vitesse_magnetique");
  // The transporting velocity is not the unknown of any equation, but the
  // convection operator queries its equation() (through MorEqn) to reach the
  // boundary-condition descriptors (Champ_Face_base::init_fcl).  Associate it
  // with this equation, exactly as the unknown is associated by the kernel.
  la_vitesse_transportante_->associer_eqn(*this);

  Equation_base::discretiser();

  Cerr << "Convection_Diffusion_Magnetic_field::discretiser() ok" << finl;
}

const Champ_Don_base& Convection_Diffusion_Magnetic_field::diffusivite_pour_transport() const
{
  if (!diffusivite_magnetique_)
    Process::exit("Convection_Diffusion_Magnetic_field: the keyword 'diffusivite_magnetique' "
                  "(magnetic diffusivity eta) is required and must be given before 'diffusion'.");
  return diffusivite_magnetique_.valeur();
}

const Champ_base& Convection_Diffusion_Magnetic_field::diffusivite_pour_pas_de_temps() const
{
  return diffusivite_pour_transport();
}

const Champ_base& Convection_Diffusion_Magnetic_field::vitesse_pour_transport() const
{
  if (!la_vitesse_transportante_)
    Process::exit("Convection_Diffusion_Magnetic_field: transporting velocity not discretized.");
  return la_vitesse_transportante_.valeur();
}

/*! @brief Reuses the kernel scalar (thermal) boundary conditions.
 *
 *  Reporting "Thermique" makes the standard scalar CLs compatible
 *  (Cond_lim_base::compatible_avec_eqn).  Dedicated magnetic CLs are deferred.
 */
const Motcle& Convection_Diffusion_Magnetic_field::domaine_application() const
{
  static Motcle domaine = "Thermique";
  return domaine;
}
