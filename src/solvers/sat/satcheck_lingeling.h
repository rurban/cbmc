/*******************************************************************\

Module:

Author: Michael Tautschnig, michael.tautschnig@cs.ox.ac.uk

\*******************************************************************/


#ifndef CPROVER_SOLVERS_SAT_SATCHECK_LINGELING_H
#define CPROVER_SOLVERS_SAT_SATCHECK_LINGELING_H

#include "cnf.h"

// NOLINTNEXTLINE(readability/identifiers)
struct LGL;

class satcheck_lingelingt:public cnf_solvert
{
public:
  satcheck_lingelingt(message_handlert &message_handler);
  virtual ~satcheck_lingelingt();

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
  void set_frozen(literalt a) override;

protected:
  resultt do_prop_solve(const bvt &assumptions) override;

  // NOLINTNEXTLINE(readability/identifiers)
  struct LGL *solver;
};

#endif // CPROVER_SOLVERS_SAT_SATCHECK_LINGELING_H
