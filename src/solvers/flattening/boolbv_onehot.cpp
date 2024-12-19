/*******************************************************************\

Module:

Author: Daniel Kroening, kroening@kroening.com

\*******************************************************************/


#include "boolbv.h"

literalt boolbvt::convert_onehot(const unary_exprt &expr)
{
  PRECONDITION(expr.id() == ID_onehot || expr.id() == ID_onehot0);

  bvt op=convert_bv(expr.op());

  // onehot0 is the same as onehot with the input bits flipped
  if(expr.id() == ID_onehot0)
    op = bv_utils.inverted(op);

  literalt one_seen=const_literal(false);
  literalt more_than_one_seen=const_literal(false);

  for(bvt::const_iterator it=op.begin(); it!=op.end(); it++)
  {
    more_than_one_seen=
      prop.lor(more_than_one_seen, prop.land(*it, one_seen));
    one_seen=prop.lor(*it, one_seen);
  }

  return prop.land(one_seen, !more_than_one_seen);
}
