/****************************************************************************
 * trust-LiMe — unit tests for the M1 induction source term object.
 *
 * These pin the data-file contract of Terme_Source_Induction_stretching: the
 * keyword it answers to, and the fact that readOn() consumes its own braces so
 * that Sources::readOn() finds the ',' or '}' it expects.
 *
 * The arithmetic of the term itself is covered by unit_induction_kernel.cpp.
 *
 * See tests/UnitTests/README.txt for how to build and run.
 ****************************************************************************/

#pragma GCC diagnostic push
#if __GNUC__ < 9
#pragma GCC diagnostic ignored "-Wsuggest-override"
#endif
#include <gtest/gtest.h>
#pragma GCC diagnostic pop

#include <Terme_Source_Induction_stretching.h>
#include <EChaineJDD.h>
#include <Nom.h>

/*! The data-file keyword must stay "induction_stretching": renaming it
 *  silently breaks every `sources { induction_stretching { } }` block.
 */
TEST(InductionStretching, TypeIsRegisteredUnderItsDataFileKeyword)
{
  Terme_Source_Induction_stretching src;
  EXPECT_STREQ(src.le_type(), "induction_stretching");
}

/*! Regression test for the parsing contract.
 *
 *  Sources::readOn() does `is >> source` (which dispatches to readOn) and then
 *  immediately expects ',' or '}'. A readOn() that returns without consuming its
 *  "{ }" block leaves the brace in the stream and breaks the whole
 *  `sources { ... }` block. A sentinel token after the block makes the stream
 *  position observable.
 */
TEST(InductionStretching, ReadOnConsumesItsOwnBraces)
{
  EChaineJDD e("induction_stretching { } SENTINEL");
  Terme_Source_Induction_stretching src;
  e >> src;

  Nom token;
  e >> token;
  EXPECT_EQ(token.getString(), std::string("SENTINEL"))
      << "readOn() must consume its own brace block, otherwise Sources::readOn "
         "would read '}' as the next token and fail";
}

/*! The source carries no keyword of its own (the velocity comes from the
 *  equation), so anything else must be rejected rather than silently ignored.
 */
TEST(InductionStretchingDeath, RejectsAnUnknownKeyword)
{
  EXPECT_DEATH(
    {
      EChaineJDD e("{ unknown_keyword 1 }");
      Terme_Source_Induction_stretching src;
      e >> src;
    },
    "");
}