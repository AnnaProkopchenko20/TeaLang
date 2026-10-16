#pragma once
#include "node.h"
#include "stmt_node.h"

struct ProgramNode : Node {
    vector<unique_ptr<StmtNode>> stmts;

    ProgramNode(int l, int c, vector<unique_ptr<StmtNode>> s)
        : Node(l, c), stmts(std::move(s)) {}

    void dump(int d) const override
    {
        indent(d); std::cout << "Program\n";
        for (auto& s : stmts)
        {
            s->dump(d + 1);
        }
    }

    llvm::Value* accept(Visitor& v) override
    {
        return v.visit_program(*this);
    }
};
