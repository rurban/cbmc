/*******************************************************************\

Module:

Author: Michael Tautschnig, michael.tautschnig@cs.ox.ac.uk

\*******************************************************************/

#include "satcheck_lingeling.h"

#include <algorithm>

#include <util/invariant.h>
#include <util/threeval.h>

extern "C"
{
#include <lglib.h>
}

#ifndef HAVE_LINGELING
#error "Expected HAVE_LINGELING"
#endif

tvt satcheck_lingelingt::l_get(literalt a) const
{
  if(a.is_constant())
    return tvt(a.sign());

  tvt result;

  if(a.var_no() > (unsigned)lglmaxvar(solver))
    return tvt(tvt::tv_enumt::TV_UNKNOWN);

  const int val = lglderef(solver, a.dimacs());
  if(val > 0)
    result = tvt(true);
  else if(val < 0)
    result = tvt(false);
  else
    return tvt(tvt::tv_enumt::TV_UNKNOWN);

  return result;
}

std::string satcheck_lingelingt::solver_text() const
{
  return "Lingeling";
}

void satcheck_lingelingt::lcnf(const bvt &bv)
{
  bvt new_bv;

  if(process_clause(bv, new_bv))
    return;

  for(const auto &literal : new_bv)
    lgladd(solver, literal.dimacs());

  lgladd(solver, 0);

  clause_counter++;
}

propt::resultt satcheck_lingelingt::do_prop_solve(const bvt &assumptions)
{
  PRECONDITION(status != statust::ERROR);

  // We start counting at 1, thus there is one variable fewer.
  {
    std::string msg =
      std::to_string(no_variables() - 1) + " variables, " +
      std::to_string(clause_counter) + " clauses";
    log.statistics() << msg << messaget::eom;
  }

  std::string msg;

  for(const auto &literal : assumptions)
    if(!literal.is_true())
      lglassume(solver, literal.dimacs());

  const int res = lglsat(solver);
  CHECK_RETURN(res == 10 || res == 20);

  if(res==10)
  {
    msg = "SAT checker: instance is SATISFIABLE";
    log.status() << msg << messaget::eom;
    status = statust::SAT;
    return resultt::P_SATISFIABLE;
  }
  else
  {
    INVARIANT(res == 20, "result value is either 10 or 20");
    msg = "SAT checker: instance is UNSATISFIABLE";
    log.status() << msg << messaget::eom;
  }

  status = statust::UNSAT;
  return resultt::P_UNSATISFIABLE;
}

void satcheck_lingelingt::set_assignment(literalt a, bool value)
{
  UNREACHABLE;
}

satcheck_lingelingt::satcheck_lingelingt(message_handlert &message_handler)
  : cnf_solvert(message_handler), solver(lglinit())
{
}

satcheck_lingelingt::~satcheck_lingelingt()
{
  lglrelease(solver);
  solver = nullptr;
}

void satcheck_lingelingt::set_frozen(literalt a)
{
  if(!a.is_constant())
    lglfreeze(solver, a.dimacs());
}

/// Returns true if an assumed literal is in conflict if the formula is UNSAT.
///
/// NOTE: if the literal is not in the assumption it causes an
/// assertion failure in lingeling.
bool satcheck_lingelingt::is_in_conflict(literalt a) const
{
  PRECONDITION(!a.is_constant());
  return lglfailed(solver, a.dimacs()) != 0;
}
