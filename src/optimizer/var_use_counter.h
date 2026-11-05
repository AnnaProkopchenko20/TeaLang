#pragma once
#include <map>
#include <set>

#include "optimizer/effects.h"
#include "optimizer/expr_util.h"
#include "optimizer/walk_visitor.h"

// Counts, per DECLARATION (not per name), the reads that resolve to it.
// Fresh counter every round: the maps are keyed by pointers.
//  * The variable declared in a `для` header is never a candidate.
//  * The target of a `для` step is counted as read (the step is not in a
//    statement list, so the pruner could not remove it together with its variable).
class VariableUseCounter : public WalkVisitor
{
public:
    explicit VariableUseCounter(const EffectOracle& o) : oracle(o) {}

    llvm::Value* visit_decl(DeclNode& n) override
    {
        if (!loop_vars.count(&n))
        {
            candidates.insert(&n);
            reads.try_emplace(&n, 0);
        }
        if (n.expr)
        {
            for (auto& item : n.expr->items)
            {
                if (item && oracle.has_effect(*item)) { reads[&n] += 1; break; }
            }
            n.expr->accept(*this);
        }
        return nullptr;
    }

    llvm::Value* visit_for(ForNode& n) override
    {
        if (n.init) loop_vars.insert(n.init.get());
        if (n.step)
        {
            if (DeclNode* d = root_decl(n.step->lhs.get())) reads[d] += 1;
        }
        return WalkVisitor::visit_for(n);
    }

    llvm::Value* visit_var(VarNameNode& n) override
    {
        if (n.decl) reads[n.decl] += 1;
        return nullptr;
    }

    // The target of an assignment is not a read. Exception: a value that may be
    // observed keeps the declaration of its target.
    llvm::Value* visit_asmt(AsmtNode& n) override
    {
        if (n.rhs && oracle.has_effect(*n.rhs))
        {
            if (DeclNode* d = root_decl(n.lhs.get())) reads[d] += 1;
        }
        if (n.rhs) n.rhs->accept(*this);
        return nullptr;
    }

    std::set<const DeclNode*> dead() const
    {
        std::set<const DeclNode*> out;
        for (const DeclNode* d : candidates)
        {
            auto it = reads.find(d);
            if (it == reads.end() || it->second == 0) out.insert(d);
        }
        return out;
    }

private:
    const EffectOracle& oracle;
    std::map<const DeclNode*, int> reads;
    std::set<const DeclNode*> candidates;
    std::set<const DeclNode*> loop_vars;
};
