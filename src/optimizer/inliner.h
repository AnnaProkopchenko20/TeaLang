#pragma once
#include <map>

#include "ast.h"
#include "optimizer/effects.h"

// Inlines AT MOST ONE call per invocation.
// A global function F is inlined when:
//   * it has exactly one use in the whole program (calls and spawns counted)
//   * it is not recursive (not in a call-graph cycle)
//   * its parameters and result are i32 / i64 / bool (no struct inlining)
//   * its body is only declarations, assignments, prints and ONE trailing exit
//     (no якщо, допоки, для, spawn, join, nested exit)
//   * the use is a plain call that is the only call or read() of its statement,
//     and the statement is a declaration, assignment, print, exit or an `якщо`
//     condition (never a допоки or для condition/step, never a spawn)
// Hoisting: the parameter declarations, the cloned body and a "ret" declaration
// are inserted just before the statement; the call becomes a read of "ret".
// Inlined locals are named F.<id>.<name> ('.' cannot occur in a program identifier).
int inline_one(ProgramNode& program,
               const std::map<const FunctionDeclNode*, int>& uses,
               const EffectOracle& oracle,
               int& counter);
