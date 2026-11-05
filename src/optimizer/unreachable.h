#pragma once
#include <vector>

#include "optimizer/walk_visitor.h"

// Drops every statement that follows a direct `чайна_перерва` in the same list.
// An `якщо`/`інакше` whose branches both exit does NOT count (same rule as the
// checker's warning). The last statement of a body stays an exit.
class UnreachableStripper : public WalkVisitor
{
public:
    int removed = 0;

    llvm::Value* visit_block(BlockNode& n) override
    {
        trim(n.stmts);
        return WalkVisitor::visit_block(n);
    }

    llvm::Value* visit_program(ProgramNode& n) override
    {
        trim(n.stmts);
        return WalkVisitor::visit_program(n);
    }

private:
    void trim(std::vector<std::unique_ptr<StmtNode>>& v)
    {
        for (size_t i = 0; i < v.size(); ++i)
        {
            if (dynamic_cast<ExitNode*>(v[i].get()))
            {
                removed += (int)(v.size() - i - 1);
                v.erase(v.begin() + (long)i + 1, v.end());
                return;
            }
        }
    }
};
