/****************************************************************************
 * trust-LiMe — Terme_Source_Lorentz implementation.
 ****************************************************************************/

#include <Terme_Source_Lorentz.h>
#include <Convection_Diffusion_Magnetic_field.h>
#include <MHD_posttraitement_kernel.h>
#include <Domaine_PolyMAC_CDO.h>
#include <Domaine_PolyMAC_MPFA.h>
#include <Domaine_Cl_PolyMAC_family.h>
#include <Champ_Face_PolyMAC_CDO.h>
#include <Probleme_base.h>
#include <Equation_base.h>
#include <Milieu_base.h>
#include <Champ_Inc_base.h>
#include <Param.h>
#include <Process.h>
#include <Domaine_dis_base.h>
#include <Domaine_Cl_dis_base.h>
#include <Synonyme_info.h>

Implemente_instanciable(Terme_Source_Lorentz, "force_lorentz_Face_PolyMAC_CDO", Source_base);
// The Sources machinery types a source as <keyword> + suffix of the velocity
// unknown class (Discretisation_base::get_name_of_type_for): on Polymac_p0
// the NS unknown is Champ_Face_PolyMAC_MPFA, so the data-file keyword
// 'force_lorentz' resolves to 'force_lorentz_Face_PolyMAC_MPFA'.  Register
// the whole PolyMAC family, like Terme_Boussinesq_PolyMAC_CDO_Face.
Add_synonym(Terme_Source_Lorentz, "force_lorentz_Face_PolyMAC_MPFA");
Add_synonym(Terme_Source_Lorentz, "force_lorentz_Face_PolyMAC_HFV");
Add_synonym(Terme_Source_Lorentz, "force_lorentz");
// XD force_lorentz source_base force_lorentz INHERITS_BRACE Lorentz force density J x B (low-Rm MHD coupling)
// XD_CONT applied to the momentum equation, computed from the magnetic field solved by the
// XD_CONT Convection_Diffusion_Magnetic_field equation of the same coupled problem.
// XD attr mu0 floattant mu0 OPT Magnetic permeability (default: the mu0 of the magnetic equation, itself 1 by default).

/*! @brief Writes the object identity (standard TRUST I/O). */
Sortie& Terme_Source_Lorentz::printOn(Sortie& os) const
{
  return os << que_suis_je();
}

/*! @brief Reads the source block: a single optional `mu0` keyword. */
Entree& Terme_Source_Lorentz::readOn(Entree& is)
{
  Param param(que_suis_je());
  Cerr << "Reading the Lorentz-force source term parameters." << finl;
  set_param(param);
  param.lire_avec_accolades_depuis(is);
  return is;
}

/*! @brief Registers the source's keywords (only the non-standard `mu0`). */
void Terme_Source_Lorentz::set_param(Param& param) const
{
  param.ajouter_non_std("mu0", this);
}

/*! @brief Reads the non-standard `mu0` keyword.
 *
 *  @param[in] mot  the keyword read from the data file.
 *  @param[in,out] is the input stream (consumed when the keyword is ours).
 *  @return 1 when the keyword was consumed, -1 to let the caller handle it.
 */
int Terme_Source_Lorentz::lire_motcle_non_standard(const Motcle& mot, Entree& is)
{
  if (mot == "mu0")
    {
      is >> mu0_;
      if (mu0_ <= 0.)
        Process::exit("Terme_Source_Lorentz: mu0 must be strictly positive.");
      return 1;
    }
  return -1;  // not ours
}

/*! @brief Effective magnetic permeability: the local `mu0` keyword when
 *         given, else the magnetic equation's own mu0.
 */
double Terme_Source_Lorentz::mu0() const
{
  return mu0_ > 0 ? mu0_ : eq_magnetic_->mu0();
}

/*! @brief Locate the magnetic equation of the hosting problem.
 *
 *  Same pattern as Terme_Boussinesq_base::associer_pb(): walk the problem's
 *  equations and keep the one of type Convection_Diffusion_Magnetic_field
 *  (the unknown is named "magnetic_field").  The source refuses to attach to
 *  a problem without a magnetic equation.
 */
void Terme_Source_Lorentz::associer_pb(const Probleme_base& pb)
{
  int ok = 0;
  const int n_eq = pb.nombre_d_equations();
  for (int i = 0; i < n_eq; i++)
    {
      const Equation_base& eqn = pb.equation(i);
      if (sub_type(Convection_Diffusion_Magnetic_field, eqn))
        {
          ok = 1;
          eq_magnetic_ = ref_cast(Convection_Diffusion_Magnetic_field, eqn);
        }
    }
  if (!ok)
    {
      Cerr << "Error in Terme_Source_Lorentz::associer_pb: the 'force_lorentz' source" << finl;
      Cerr << "requires a Convection_Diffusion_Magnetic_field equation in the same" << finl;
      Cerr << "problem, but problem " << pb.le_nom() << " of type " << pb.que_suis_je() << finl;
      Cerr << "has none. Add the magnetic equation to the Pb_MHD / Pb_MHD_Turbulent problem block." << finl;
      Process::exit();
    }
}

/*! @brief Grabs the PolyMAC discretisation domains (cast to the CDO/family
 *         types used by the assembly).
 */
void Terme_Source_Lorentz::associer_domaines(const Domaine_dis_base& domaine_dis,
                                             const Domaine_Cl_dis_base& domaine_Cl_dis)
{
  le_dom_PolyMAC_ = ref_cast(Domaine_PolyMAC_CDO, domaine_dis);
  le_dom_Cl_PolyMAC_ = ref_cast(Domaine_Cl_PolyMAC_family, domaine_Cl_dis);
}

/*! @brief Adds the Lorentz force per unit mass to the momentum second member.
 *
 *  Per-element force (lagged b):  F_e = (curl b_e x b_e) / mu0 / rho_e,
 *  with curl b from the Green-Gauss element gradient of b.  The force is
 *  then projected on the velocity unknown following the kernel
 *  Terme_Boussinesq_PolyMAC_CDO_Face structure:
 *
 *   - face block: secmem(f) += (F_e . n_f) / S_f * Vf_d(f,i) * poro_f(f)
 *     for each adjacent element e (both upstream and downstream, as in the
 *     kernel source);
 *   - element block (PolyMAC_MPFA auxiliary velocity variables only):
 *     secmem(nf_tot + dim*e + d) += F_e(d) * poro_e(e) * V_e;
 *   - for MPFA, faces with a Dirichlet velocity CL (fcl(f,0) >= 2) are
 *     skipped in the face block (their momentum equation is replaced by the
 *     CL; the element block carries the force).
 */
void Terme_Source_Lorentz::ajouter_blocs(matrices_t matrices, DoubleTab& secmem,
                                         const tabs_t& semi_impl) const
{
  const Domaine_PolyMAC_CDO& dom = le_dom_PolyMAC_.valeur();
  const int D = dimension;
  const int ne = dom.nb_elem_tot();
  const int nf_tot = dom.nb_faces_tot();
  const bool mpfa = sub_type(Domaine_PolyMAC_MPFA, dom);

  // Magnetic field: in strong-coupling mode read the latest (futur) values
  // so the Lorentz force uses the updated field within the Picard sweep.
  const DoubleTab& b = eq_magnetic_->couplage_fort() ? eq_magnetic_->inconnue().futur()
                       : eq_magnetic_->inconnue().valeurs();
  const int nb_b = eq_magnetic_->nb_composantes();
  DoubleTrav gb;
  eq_magnetic_->gradient_b_elements(gb);

  // Fluid density (uniform or per-element layouts both accepted).
  const DoubleTab& rho = equation().milieu().masse_volumique().valeurs();
  const auto rho_elem = [&rho](int e) -> double
  { return rho.dimension(0) == 1 ? rho(0, 0) : rho(e, 0); };

  const double la_mu0 = mu0();
  double w[3] = { 0., 0., 0. }, s[3] = { 0., 0., 0. };

  // Per-element force per unit mass.
  DoubleTrav F(ne, D);
  F = 0.;
  for (int e = 0; e < ne; e++)
    {
      mhd_curl_b(&gb(e, 0), D, w);
      mhd_force_lorentz(w, &b(e, 0), nb_b, D, la_mu0, s);
      const double irho = 1. / rho_elem(e);
      for (int d = 0; d < D; d++)
        F(e, d) = s[d] * irho;
    }

  // Geometric / porosity data.
  const IntTab& f_e = dom.face_voisins();
  const DoubleTab& nf = dom.face_normales();
  const DoubleVect& fs = dom.face_surfaces();
  const DoubleTab& vfd = dom.volumes_entrelaces_dir();
  const DoubleVect& pf = equation().milieu().porosite_face();
  const DoubleVect& pe = equation().milieu().porosite_elem();
  const DoubleVect& ve = dom.volumes();

  // Face block.  For MPFA the velocity unknown carries CL descriptors:
  // skip Dirichlet-velocity faces (fcl(f,0) >= 2), like the kernel source.
  const IntTab* fcl = nullptr;
  if (mpfa)
    fcl = &ref_cast(Champ_Face_PolyMAC_CDO, equation().inconnue()).fcl();

  for (int f = 0; f < dom.nb_faces(); f++)
    {
      if (fcl != nullptr && (*fcl)(f, 0) >= 2)
        continue;
      for (int i = 0; i < 2 && (f_e(f, i)) >= 0; i++)
        {
          const int e = f_e(f, i);
          double fn = 0.;
          for (int d = 0; d < D; d++)
            fn += F(e, d) * nf(f, d);
          secmem(f) += fn / fs(f) * vfd(f, i) * pf(f);
        }
    }

  // Element block: auxiliary element velocity variables of the MPFA
  // face-field unknown.
  if (mpfa)
    for (int e = 0; e < ne; e++)
      for (int d = 0; d < D; d++)
        secmem(nf_tot + D * e + d) += F(e, d) * pe(e) * ve(e);
}
