/*******************************************************************\

Module:

Author: Daniel Kroening, kroening@kroening.com

\*******************************************************************/

#include "boolbv.h"

#include <util/bitvector_expr.h>

bvt boolbvt::convert_bitwise(const exprt &expr)
{
  const std::size_t width = boolbv_width(expr.type());

  if(expr.id()==ID_bitnot)
  {
    const exprt &op = to_bitnot_expr(expr).op();
    const bvt &op_bv = convert_bv(op, width);
    return bv_utils.inverted(op_bv);
  }
  else if(expr.id()==ID_bitand || expr.id()==ID_bitor ||
          expr.id()==ID_bitxor ||
          expr.id()==ID_bitnand || expr.id()==ID_bitnor ||
          expr.id()==ID_bitxnor)
  {
    std::function<literalt(literalt, literalt)> f;

    if(expr.id() == ID_bitand || expr.id() == ID_bitnand)
      f = [this](literalt a, literalt b) { return prop.land(a, b); };
    else if(expr.id() == ID_bitor || expr.id() == ID_bitnor)
      f = [this](literalt a, literalt b) { return prop.lor(a, b); };
    else if(expr.id() == ID_bitxor || expr.id() == ID_bitxnor)
      f = [this](literalt a, literalt b) { return prop.lxor(a, b); };
    else
      UNIMPLEMENTED;

    bvt bv;
    bv.resize(width);

    forall_operands(it, expr)
    {
      const bvt &op = convert_bv(*it, width);

      if(it==expr.operands().begin())
        bv=op;
      else
      {
        for(std::size_t i=0; i<width; i++)
        {
          bv[i] = f(bv[i], op[i]);
        }
      }
    }

    if(
      expr.id() == ID_bitnand || expr.id() == ID_bitnor ||
      expr.id() == ID_bitxnor)
    {
      return bv_utils.inverted(bv);
    }
    else
      return bv;
  }

  UNIMPLEMENTED;
}
