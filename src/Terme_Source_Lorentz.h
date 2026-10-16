/****************************************************************************
 * trust-LiMe — Lorentz force source term for the momentum equation.
 *
 * Terme_Source_Lorentz is a Source_base attached to the Navier-Stokes
 * equation of a Pb_MHD problem that also solves the magnetic induction
 * equation (the Navier-Stokes equation is then an optional extra equation
 * of Pb_MHD, see Pb_MHD.h).  It adds the low-Rm MHD coupling: the magnetic
 * tension-free part of the Lorentz force density
 *
 *     F = J x B,     J = curl(b) / mu0,
 *
 * computed from the magnetic field solved by the *other* equation of the
 * same problem (found by unknown name "magnetic_field", the Terme_Boussinesq
 * pattern).  b is lagged (the source is explicit: dimensionner_blocs()
 * contributes nothing to the Jacobian), consistent with the kinematic
 * low-Rm approximation of the coupled plan (PLAN_mhd_coupled.md, D6).
 *
 * The force is divided by the fluid density: like every TRUST Navier-Stokes
 * volumetric source (gravity/Boussinesq precedent), the second member is a
 * force per unit mass.
 *
 * curl(b) comes from the Green-Gauss element gradient of b reconstructed by
 * Convection_Diffusion_Magnetic_field::gradient_b_elements() — exact for
 * linear fields at interior cells, O(h) in wall-adjacent cells (homogeneous
 * Neumann wall assumption).  The same reconstruction is validated by the
 * tension_magnetique / rotationnel_magnetique / courant_magnetique
 * post-treatment fields and by tests/UnitTests.
 *
 * Assembly follows the kernel Terme_Boussinesq_PolyMAC_CDO_Face structure
 * for the PolyMAC family: the face block of the face-based velocity unknown
 * receives the normal projection of the force weighted by the upstream
 * entrelace volumes and the face porosity, and — for PolyMAC_MPFA
 * (Polymac_p0), whose velocity unknown carries auxiliary element variables —
 * the element block receives the force times the element porosity and
 * volume.  Dirichlet-velocity faces (fcl(f,0) >= 2) are skipped in the face
 * block for MPFA, exactly as in the kernel source.
 ****************************************************************************/

#ifndef Terme_Source_Lorentz_included
#define Terme_Source_Lorentz_included

#include <Source_base.h>
#include <TRUST_Ref.h>

class Convection_Diffusion_Magnetic_field;
class Domaine_PolyMAC_CDO;
class Domaine_Cl_PolyMAC_family;
class Param;

/*! @brief Lorentz force source (J x B) coupling the magnetic equation to the
 *         momentum equation of the same Pb_MHD problem.
 *
 *  Data-file usage, inside the Navier-Stokes equation block of a Pb_MHD
 *  problem that also contains Convection_Diffusion_Magnetic_field:
 *
 *      Navier_Stokes_std
 *      {
 *          ...
 *          sources { force_lorentz { } }
 *      }
 *
 *  @sa Source_base, Convection_Diffusion_Magnetic_field, Pb_MHD
 */
class Terme_Source_Lorentz : public Source_base
{
  Declare_instanciable(Terme_Source_Lorentz);

public:

  // Explicit (lagged b) source: nothing in the Jacobian.
  int has_interface_blocs() const override { return 1; }
  void dimensionner_blocs(matrices_t matrices, const tabs_t& semi_impl = {}) const override { }
  void ajouter_blocs(matrices_t matrices, DoubleTab& secmem, const tabs_t& semi_impl = {}) const override;

  // Locate the magnetic equation of the problem (by unknown name).
  void associer_pb(const Probleme_base&) override;

  // Nothing time-dependent here (b is re-read in ajouter_blocs at every
  // call); the override only satisfies the Source_base runtime check.
  void mettre_a_jour(double temps) override { }

  // Single-phase only in this phase; no multiphase check needed.
  void check_multiphase_compatibility() const override { }

  // Effective magnetic permeability: the mu0 keyword when given, else the
  // magnetic equation's own mu0.
  double mu0() const;

protected:

  // Optional keyword: mu0 <value> (overrides the magnetic equation's mu0).
  int lire_motcle_non_standard(const Motcle&, Entree&) override;
  void set_param(Param& param) const override;

  // Grab the PolyMAC discretization domains.
  void associer_domaines(const Domaine_dis_base&, const Domaine_Cl_dis_base&) override;

  // The magnetic equation solved by the same problem (observed, not owned).
  OBS_PTR(Convection_Diffusion_Magnetic_field) eq_magnetic_;
  OBS_PTR(Domaine_PolyMAC_CDO) le_dom_PolyMAC_;
  OBS_PTR(Domaine_Cl_PolyMAC_family) le_dom_Cl_PolyMAC_;
  double mu0_ = -1.;   // <0: use the magnetic equation's mu0
};

#endif /* Terme_Source_Lorentz_included */
