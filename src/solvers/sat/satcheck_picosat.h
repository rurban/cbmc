/*******************************************************************\

Module:

Author: Michael Tautschnig, michael.tautschnig@cs.ox.ac.uk

\*******************************************************************/


#ifndef CPROVER_SOLVERS_SAT_SATCHECK_PICOSAT_H
#define CPROVER_SOLVERS_SAT_SATCHECK_PICOSAT_H

#include "cnf.h"

// NOLINTNEXTLINE(readability/identifiers)
struct PicoSAT;

class satcheck_picosatt:public cnf_solvert
{
public:
  satcheck_picosatt(message_handlert &message_handler);
  ~satcheck_picosatt();

  std::string solver_text() const override;
  tvt l_get(literalt a) const override;

  void lcnf(const bvt &bv) override;
  void set_assignment(literalt a, bool value) override;

  bool is_in_conflict(literalt a) const override;
  bool has_assumptions() const override
  {
    return true;
  }
  bool has_is_in_conflict() const override
  {
    return true;
  }

protected:
  resultt do_prop_solve(const bvt &assumptions) override;

private:
  PicoSAT *picosat;
};

#endif // CPROVER_SOLVERS_SAT_SATCHECK_PICOSAT_H
