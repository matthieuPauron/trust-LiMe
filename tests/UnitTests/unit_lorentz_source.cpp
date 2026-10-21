/****************************************************************************
 * trust-LiMe — unit tests for the Lorentz force source term object.
 *
 * These pin the data-file contract of Terme_Source_Lorentz (the keyword it
 * answers to, its `mu0` override, and its problem-binding rules) and the
 * error paths that the ErrorInputs datasets only exercise end-to-end.
 *
 * The arithmetic of the force itself (F = curl(b) x b / mu0) is covered by
 * unit_mhd_posttraitement_kernel.cpp, and the assembled physics by the
 * MagneticCoupled validation forms (ForceLorentzBalance, MMS_Coupled,
 * Coupled_Feedback, HartmannTwoWay).
 *
 * See tests/UnitTests/README.txt for how to build and run.
 ****************************************************************************/

#pragma GCC diagnostic push
#if __GNUC__ < 9
#pragma GCC diagnostic ignored "-Wsuggest-override"
#endif
#include <gtest/gtest.h>
#pragma GCC diagnostic pop

#include <Terme_Source_Lorentz.h>
#include <Convection_Diffusion_Magnetic_field.h>
#include <Pb_MHD.h>
#include <Pb_Hydraulique.h>
#include <EChaineJDD.h>
#include <Nom.h>

/*! The primary registration must stay "force_lorentz_Face_PolyMAC_CDO": the
 *  data-file keyword `force_lorentz` resolves to it through the source
 *  typing machinery (<keyword> + suffix of the velocity unknown class), and
 *  the MPFA/HFV spellings are Add_synonym aliases of it. Renaming the
 *  primary silently breaks every `sources { force_lorentz { } }` block.
 */
TEST(LorentzSource, TypeIsRegisteredUnderItsPrimaryKeyword)
{
  Terme_Source_Lorentz src;
  EXPECT_STREQ(src.le_type(), "force_lorentz_Face_PolyMAC_CDO");
}

/*! Regression test for the parsing contract.
 *
 *  Sources::readOn() does `is >> source` and then immediately expects ','
 *  or '}'. readOn() must consume its own "{ }" block (it delegates to
 *  Param::lire_avec_accolades_depuis); a sentinel token after the block
 *  makes the stream position observable.
 */
TEST(LorentzSource, ReadOnConsumesItsOwnBraces)
{
  EChaineJDD e("{ } SENTINEL");
  Terme_Source_Lorentz src;
  e >> src;

  Nom token;
  e >> token;
  EXPECT_EQ(token.getString(), std::string("SENTINEL"))
      << "readOn() must consume its own brace block, otherwise Sources::readOn "
         "would read '}' as the next token and fail";
}

/*! The only own keyword is `mu0`: when given it overrides the magnetic
 *  equation's permeability in mu0() (used by ajouter_blocs).
 */
TEST(LorentzSource, Mu0KeywordOverridesTheEquationDefault)
{
  EChaineJDD e("{ mu0 2.5 } SENTINEL");
  Terme_Source_Lorentz src;
  e >> src;
  EXPECT_DOUBLE_EQ(src.mu0(), 2.5);
}

/*! Without the keyword, mu0() falls back to the magnetic equation's own
 *  mu0 (1 by default). After associer_pb() on a Pb_MHD the fallback proves
 *  the magnetic equation was located and bound: an unbound source would
 *  dereference a null observer here.
 */
TEST(LorentzSource, Mu0FallsBackToTheBoundMagneticEquation)
{
  Pb_MHD pb;
  Terme_Source_Lorentz src;
  src.associer_pb(pb);   // must find the magnetic equation of equation 0
  EXPECT_DOUBLE_EQ(src.mu0(), 1.0);   // the equation's default mu0
}

/*! The `mu0` keyword must be strictly positive: a zero or negative value
 *  would silently invert or blow up the force.
 */
TEST(LorentzSourceDeath, RejectsANonPositiveMu0)
{
  EXPECT_DEATH(
    {
      EChaineJDD e("{ mu0 -1. }");
      Terme_Source_Lorentz src;
      e >> src;
    },
    "");
}

/*! Anything else in the block must be rejected rather than silently
 *  ignored (a typo'd keyword would leave the source at its defaults).
 */
TEST(LorentzSourceDeath, RejectsAnUnknownKeyword)
{
  EXPECT_DEATH(
    {
      EChaineJDD e("{ unknown_keyword 1 }");
      Terme_Source_Lorentz src;
      e >> src;
    },
    "");
}

/*! The source refuses to attach to a problem without a magnetic equation
 *  (the unit-level counterpart of the ErrorInputs `error_bad_force`
 *  dataset: a `force_lorentz` on a pure hydraulic problem must fail with a
 *  readable diagnostic, not assemble garbage).
 */
TEST(LorentzSourceDeath, RefusesAProblemWithoutAMagneticEquation)
{
  EXPECT_DEATH(
    {
      Pb_Hydraulique pb;   // holds only Navier_Stokes_std
      Terme_Source_Lorentz src;
      src.associer_pb(pb);
    },
    "");
}
