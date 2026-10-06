#pragma once
#include "expr_node.h"
#include "../keywords.h"
#include "stmt_node.h"

struct ExitNode : StmtNode {
    unique_ptr<ExprNode> expr;
    ExitNode(int l, int c, unique_ptr<ExprNode> e) : StmtNode(l, c), expr(std::move(e)) {}

    void dump(int d) const override
    {
        indent(d); std::cout << kw::EXIT << "\n"; expr->dump(d + 1);
    }

    llvm::Value* accept(Visitor& v) override
    {
        return v.visit_exit(*this);
    }
};
