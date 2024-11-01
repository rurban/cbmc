/*******************************************************************\

Module:

Author: Reini Urban

\*******************************************************************/

#ifdef HAVE_SATCH

#  include "satcheck_satch.h"

#  include <util/exception_utils.h>
#  include <util/invariant.h>
#  include <util/narrow.h>
#  include <util/threeval.h>

#  include <satch.h>

// from satch/main.c

// This parser for DIMACS files is meant to be pretty robust and precise.
// For instance it carefully checks that the number of variables as well as
// literals are valid 32-two bit integers (different from 'INT_MIN'). For
// the number of clauses it uses 'size_t'.  Thus in a 64-bit environment it
// can parse really large CNFs with 2^32 clauses and more.

// The following function reads a character from the global input file,
// squeezes out carriage return characters (after checking that they are
// followed by a newline) and maintains read bytes and lines statistics.

static inline int
next (void)
{
  int res = getc (input.file);
  if (res == '\r')		// Care for DOS / Windows '\r\n'.
    {
      bytes++;
      res = getc (input.file);
      if (res != '\n')
	parse_error ("expected new line after carriage return");
    }
  if (res == '\n')
    lineno++;
  if (res != EOF)
    bytes++;
  return res;
}

// Needed at several places to print statistics.

static inline double
percent (double a, double b)
{
  return b ? 100 * a / b : 0;
}

// This is the actual DIMACS file parser.  It uses the 'next' function to
// read bytes from the global file.  Beside proper error messages in case of
// parse errors it also prints information about parsed clauses etc.

// The file is not opened here, since we want to print the 'banner' in
// 'main' after checking that we can really access and open the file.  But
// it is closed in this function to print the information just discussed at
// the right place where this should happen.

static void
parse (void)
{
  satch_start_profiling_parsing (solver);

  if (!quiet)
    {
      satch_section (solver, "parsing");
      message ("%sparsing '%s'", force ? "force " : "", input.path);
    }

  int ch;
  while ((ch = next ()) == 'c')
    {
      while ((ch = next ()) != '\n')
	if (ch == EOF)
	  parse_error ("unexpected end-of-file in header comment");
    }
  if (ch != 'p')
    parse_error ("expected 'p' or 'c'");
  if (next () != ' ')
    parse_error ("expected space after 'p'");
  char format = next ();
  if (format != 'c' && format != 'x')
    parse_error ("expected 'c' or 'x' after 'p '");
  if (next () != 'n')
    parse_error ("expected 'n' after 'p c'");
  if (next () != 'f')
    parse_error ("expected 'f' after 'p cn'");
  if (next () != ' ')
    parse_error ("expected space after 'p %cnf'", format);
  while ((ch = next ()) == ' ' || ch == '\t')
    ;
  if (!isdigit (ch))
    parse_error ("expected digit after 'p %cnf '", format);
  variables = ch - '0';
  while (isdigit (ch = next ()))
    {
      if (!variables)
	parse_error ("invalid digit after '0' "
		     "while parsing maximum variable");
      if (INT_MAX / 10 < variables)
	parse_error ("maximum variable number way too big");
      variables *= 10;
      const int digit = ch - '0';
      if (INT_MAX - digit < variables)
	parse_error ("maximum variable number too big");
      variables += digit;
    }
  if (ch != ' ')
    parse_error ("expected space after 'p %cnf %d'", format, variables);
  while ((ch = next ()) == ' ' || ch == '\t')
    ;
  if (!isdigit (ch))
    parse_error ("expected digit after 'p %cnf %d '", format, variables);
  size_t specified_clauses = ch - '0';
  while (isdigit (ch = next ()))
    {
      if (!specified_clauses)
	parse_error ("invalid digit after '0' "
		     "while parsing number of clauses");
      const size_t MAX_SIZE_T = ~(size_t) 0;
      if (MAX_SIZE_T / 10 < specified_clauses)
	parse_error ("way too many clauses specified");
      specified_clauses *= 10;
      const int digit = ch - '0';
      if (MAX_SIZE_T - digit < specified_clauses)
	parse_error ("too many clauses specified");
      specified_clauses += digit;
    }
  if (ch == ' ' || ch == '\t')
    {
      while ((ch = next ()) == ' ' || ch == '\t')
	;
    }
  if (ch != '\n')
    parse_error ("expected new line after 'p %cnf %d %zu'", format,
		 variables, specified_clauses);

  message ("parsed 'p %cnf %d %zu' header",
	   format, variables, specified_clauses);
  satch_reserve (solver, variables);

  int parsed_variables = 0;	// Maximum parsed variable index.
  size_t parsed_clauses = 0;
  size_t parsed_xors = 0;

  size_t offset_of_encoded_xors = 0;

  int tseitin = force ? 0 : variables;
  char type = 0;
  int lit = 0;

  for (;;)
    {
      ch = next ();

      // Skip white space.

      if (ch == ' ' || ch == '\t' || ch == '\n')
	continue;

      if (ch == EOF)
	break;

      // Read and skip comments.

      if (ch == 'c')
	{

	COMMENT:		// See below on why we need 'goto' here.

	  while ((ch = next ()) != '\n')
	    if (ch == EOF)
	      parse_error ("unexpected end-of-file in comment");
	  continue;
	}

      // Read XOR type.

      if (ch == 'x')
	{
	  if (lit)
	    parse_error ("'x' after non-zero %d'", lit);
	  if (type)
	    parse_error ("'x' after '%c'", type);
	  if (!force && format != 'x')
	    parse_error ("unexpected 'x' in CNF (use 'p xnf ...' header)");
	  type = 'x';
	  continue;
	}

      // Get sign of next literal and its first digit.

      int sign = 1;

      if (ch == '-')
	{
	  ch = next ();
	  if (!isdigit (ch))
	    parse_error ("expected digit after '-'");
	  if (ch == '0')
	    parse_error ("expected non-zero digit after '-'");
	  sign = -1;
	}
      else if (!isdigit (ch))
	parse_error ("expected number");

      // In forced parsing mode we ignore specified clauses.

      if (!force)
	{
	  assert (parsed_clauses <= specified_clauses);
	  if (parsed_clauses == specified_clauses)
	    parse_error ("more clauses than specified");
	}

      // Read the variable index and make sure not to overflow.

      int idx = ch - '0';
      while (isdigit (ch = next ()))
	{
	  if (!idx)
	    parse_error ("invalid digit after '0' in number");
	  if (INT_MAX / 10 < idx)
	    parse_error ("number way too large");
	  idx *= 10;
	  const int digit = ch - '0';
	  if (INT_MAX - digit < idx)
	    parse_error ("number too large");
	  idx += digit;
	}

      // Now we have the variable with its sign as parsed literal.

      lit = sign * idx;

      // Be careful to check the character after the last digit.

      if (ch != ' ' && ch != '\t' && ch != '\n' && ch != 'c' && ch != EOF)
	parse_error ("unexpected character after '%d'", lit);

      assert (lit != INT_MIN);
      if (!force && idx > variables)
	parse_error ("literal '%d' exceeds maximum variable index '%d'",
		     lit, variables);

      if (idx > parsed_variables)
	parsed_variables = idx;

      if (!lit)
	parsed_clauses++;

      if (!type)
	{
	  // The IPASIR semantics of 'satch_add' in essence just gets the
	  // numbers in the DIMACS file after the header and 'adds' them
	  // including the zeroes terminating each clause.  Thus we do not
	  // have to use another function for adding a clause explicitly.

	  satch_add (solver, lit);
	}
      else if (lit)
	{
	  assert (type == 'x');
	  PUSH (xors, lit);
	}
      else
	{
	  assert (type == 'x');
	  type = 0;

	  // As described above (before 'encode_xors'), in forced parsing
	  // mode we need to wait until we know the maximum variable in the
	  // file before we can start encoding XORs.  In precise parsing
	  // mode we can simply encode the XOR directly (which also is
	  // beneficial to activate and place Tseitin variables close to the
	  // other variables seen so far and thus in this XOR clause).

	  const size_t new_offset = SIZE_STACK (xors);
	  const size_t size = new_offset - offset_of_encoded_xors;
	  int *x = xors.begin + offset_of_encoded_xors;

	  if (force)
	    {
#ifdef LOGGING
	      if (logging_prefix ("parsed size %zu XOR", size))
		{
		  for (const int *p = x; x != xors.end; x++)
		    printf (" %d", *p);
		  logging_suffix ();
		}
#endif
	      PUSH (xors, 0);
	    }
	  else
	    {
	      tseitin = encode_xor (tseitin, size, x);
#ifndef NDEBUG
	      PUSH (xors, 0);
	      offset_of_encoded_xors = new_offset + 1;
	      assert (offset_of_encoded_xors == SIZE_STACK (xors));
#else
	      CLEAR_STACK (xors);
	      assert (!offset_of_encoded_xors);
#endif
	    }

	  parsed_xors++;
	}

      // The following 'goto' is necessary to avoid reading another
      // character which would result in a spurious parse error for a comment
      // immediately starting after a literal, e.g., as in '1comment'.

      if (ch == 'c')
	goto COMMENT;
    }

  if (lit)
    parse_error ("terminating zero after literal '%d' missing", lit);

  if (type)
    assert (format == 'x'), parse_error ("literals missing after 'x'");

  if (!force && parsed_clauses < specified_clauses)
    {
      if (parsed_clauses + 1 == specified_clauses)
	parse_error ("single clause missing");
      else
	parse_error ("%zu clauses missing",
		     specified_clauses - parsed_clauses);
    }

  // Handle delayed XOR encoding in forced parsing mode.

  if (!EMPTY_STACK (xors))
    encode_xors (parsed_variables, offset_of_encoded_xors);

  const double seconds = satch_stop_profiling_parsing (solver);

  if (parsed_clauses == 1)
    message ("parsed exactly one clause in %.2f seconds", seconds);
  else
    message ("parsed %zu clauses in %.2f seconds", parsed_clauses, seconds);

  if (parsed_xors == 1)
    message ("including exactly one XOR clause %.0f%%",
	     percent (1, parsed_clauses));
  else if (parsed_xors > 1)
    message ("including %zu XOR clauses %.0f%%",
	     parsed_xors, percent (parsed_xors, parsed_clauses));
  else if (format == 'x')
    assert (!parsed_xors), message ("without any XOR clauses");

  if (parsed_variables == 0)
    message ("input file does not contain any variable");
  else
    message ("found maximum variable index %d", parsed_variables);

  if (force && variables < parsed_variables)
    variables = parsed_variables;

  if (input.close == 1)		// Opened with 'fopen'.
    fclose (input.file);

#ifdef _POSIX_C_SOURCE
  if (input.close == 2)		// Opened with 'popen'.
    pclose (input.file);
#endif

  message ("closed '%s'", input.path);
  message ("after reading %" PRIu64 " bytes (%.0f MB)",
	   bytes, bytes / (double) (1 << 20));

#ifdef NDEBUG
  RELEASE_STACK (xors);
#endif
}

tvt satcheck_satcht::l_get(literalt a) const
{
  if(a.is_constant())
    return tvt(a.sign());

  tvt result;

  if(a.var_no() > narrow<unsigned>(solver->vars()))
    return tvt(tvt::tv_enumt::TV_UNKNOWN);

  const int val = solver->val(a.dimacs());
  if(val>0)
    result = tvt(true);
  else if(val<0)
    result = tvt(false);
  else
    return tvt(tvt::tv_enumt::TV_UNKNOWN);

  return result;
}

std::string satcheck_satcht::solver_text() const
{
  return std::string("SATCH ") + satch_version();
}

void satcheck_satcht::lcnf(const bvt &bv)
{
  for(const auto &lit : bv)
  {
    if(lit.is_true())
      return;
    else if(!lit.is_false())
      INVARIANT(lit.var_no() < no_variables(), "reject out of bound variables");
  }

  for(const auto &lit : bv)
  {
    if(!lit.is_false())
    {
      // add literal with correct sign
      solver->add(lit.dimacs());
    }
  }
  solver->add(0); // terminate clause

  if(solver_hardness)
  {
    // To map clauses to lines of program code, track clause indices in the
    // dimacs cnf output. Dimacs output is generated after processing
    // clauses to remove duplicates and clauses that are trivially true.
    // Here, a clause is checked to see if it can be thus eliminated. If
    // not, add the clause index to list of clauses in
    // solver_hardnesst::register_clause().
    static size_t cnf_clause_index = 0;
    bvt cnf;
    bool clause_removed = process_clause(bv, cnf);

    if(!clause_removed)
      cnf_clause_index++;

    solver_hardness->register_clause(
      bv, cnf, cnf_clause_index, !clause_removed);
  }

  clause_counter++;
}

propt::resultt satcheck_satcht::do_prop_solve(const bvt &assumptions)
{
  INVARIANT(status != statust::ERROR, "there cannot be an error");

  log.statistics() << (no_variables() - 1) << " variables, " << clause_counter
                   << " clauses" << messaget::eom;

  // if assumptions contains false, we need this to be UNSAT
  for(const auto &a : assumptions)
  {
    if(a.is_false())
    {
      log.status() << "got FALSE as assumption: instance is UNSATISFIABLE"
                   << messaget::eom;
      status = statust::UNSAT;
      return resultt::P_UNSATISFIABLE;
    }
  }

  for(const auto &a : assumptions)
    if(!a.is_true())
      solver->assume(a.dimacs());

  switch(solver->solve())
  {
  case 10:
    log.status() << "SAT checker: instance is SATISFIABLE" << messaget::eom;
    status = statust::SAT;
    return resultt::P_SATISFIABLE;
  case 20:
    log.status() << "SAT checker: instance is UNSATISFIABLE" << messaget::eom;
    break;
  default:
    log.status() << "SAT checker: solving returned without solution"
                 << messaget::eom;
    throw analysis_exceptiont(
      "solving inside SATCH SAT solver has been interrupted");
  }

  status = statust::UNSAT;
  return resultt::P_UNSATISFIABLE;
}

void satcheck_satcht::set_assignment(literalt a, bool value)
{
  INVARIANT(!a.is_constant(), "cannot set an assignment for a constant");
  INVARIANT(false, "method not supported");
}

satcheck_satcht::satcheck_satcht(message_handlert &message_handler)
  : cnf_solvert(message_handler), solver(satch_init())
{
  // default 0
  //satch_set_verbose_level(solver, 0);
}

satcheck_satcht::~satcheck_satcht()
{
  satch_release(solver);
}

bool satcheck_satcht::is_in_conflict(literalt a) const
{
  // return satch_conflicts(satch, a.dimacs());
}

#endif
