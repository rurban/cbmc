/*******************************************************************\

Module: Unit tests for ieee_floatt

Author: Daniel Kroening, dkr@amazon.com

\*******************************************************************/

#include <util/ieee_float.h>

#include <testing-utils/use_catch.h>

TEST_CASE("Make an IEEE 754 one", "[core][util][ieee_float]")
{
  auto spec = ieee_float_spect::single_precision();
  REQUIRE(ieee_floatt::one(spec) == 1);
}
