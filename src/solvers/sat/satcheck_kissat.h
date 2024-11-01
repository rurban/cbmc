/*******************************************************************\

Module:

Author: Reini Urban

\*******************************************************************/


#ifndef CPROVER_SOLVERS_SAT_SATCHECK_KISSAT_H
#define CPROVER_SOLVERS_SAT_SATCHECK_KISSAT_H

#include "cnf.h"

#include <solvers/hardness_collector.h>

class satcheck_kissatt : public cnf_solvert, public hardness_collectort
{
public:
  explicit satcheck_kissatt(message_handlert &message_handler);
  virtual ~satcheck_kissatt();

  std::string solver_text() const override;
  tvt l_get(literalt a) const override;

  void lcnf(const bvt &bv) override;
  void set_assignment(literalt a, bool value) override;

  bool has_assumptions() const override
  {
    return true;
  }
  bool has_is_in_conflict() const override
  {
    return true;
  }
  bool is_in_conflict(literalt a) const override;

protected:
  resultt do_prop_solve(const bvt &assumptions) override;

  kissat *solver;
};

#endif // CPROVER_SOLVERS_SAT_SATCHECK_KISSAT_H
