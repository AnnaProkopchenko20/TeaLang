#pragma once
#include "block_node.h"
#include "decl_node.h"
#include "asmt_node.h"
#include "expr_node.h"
#include "stmt_node.h"
#include "../keywords.h"

// для ( [init] ; [cond] ; [step] ) NL block
// init is a declaration, step an assignment (both without their NL).
// An empty cond means "повне". The variable of init is visible only in cond,
// step and body: the checker opens a scope for the whole statement.
struct ForNode : StmtNode {
    unique_ptr<DeclNode>  init;   // may be null
    unique_ptr<ExprNode>  cond;   // may be null
    unique_ptr<AsmtNode>  step;   // may be null
    unique_ptr<BlockNode> body;

    ForNode(int l, int c, unique_ptr<DeclNode> i, unique_ptr<ExprNode> cd,
            unique_ptr<AsmtNode> st, unique_ptr<BlockNode> b)
        : StmtNode(l, c), init(std::move(i)), cond(std::move(cd)),
          step(std::move(st)), body(std::move(b)) {}

    void dump(int d) const override
    {
        indent(d); std::cout << kw::FOR << "\n";
        if (init) init->dump(d + 1);
        if (cond) cond->dump(d + 1);
        if (step) step->dump(d + 1);
        body->dump(d + 1);
    }

    llvm::Value* accept(Visitor& v) override { return v.visit_for(*this); }
};
