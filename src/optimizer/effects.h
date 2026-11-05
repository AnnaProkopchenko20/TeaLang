#pragma once
#include <set>
#include "ast.h"

// Decides whether evaluating an expression may be observed from outside.
// A value has an effect if it contains read(), or a call of a function in the
// effectful set. The set is recomputed every round (removals change the graph).
//
// Effectful functions (global and member):
//   * the body contains print, read(), any `допоки`/`для`, a spawn or a join
//   * a mutating member function (MemberFnDeclNode::mutates)
//   * a function that can reach itself in the call graph (may not return)
//   * a function that calls an effectful function (fixpoint)
//   * a function with an unresolved call (unknown -> keep)
class EffectOracle
{
public:
    void analyse(ProgramNode& program);
    bool has_effect(const ExprNode& expr) const;
    bool is_effectful(const FunctionDeclNode* fn) const { return !fn || effectful_.count(fn) > 0; }
    bool is_recursive(const FunctionDeclNode* fn) const { return cyclic_.count(fn) > 0; }

private:
    std::set<const FunctionDeclNode*> effectful_;
    std::set<const FunctionDeclNode*> cyclic_;
};
