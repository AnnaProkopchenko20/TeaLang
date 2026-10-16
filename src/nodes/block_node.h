#pragma once
#include "node.h"
#include "stmt_node.h"

struct BlockNode : Node {
    vector<unique_ptr<StmtNode>> stmts;

    BlockNode(int l, int c, vector<unique_ptr<StmtNode>> s)
        : Node(l, c), stmts(std::move(s)) {}

    void dump(int d) const override
    {
        indent(d); std::cout << "Block\n";
        for (auto& s : stmts)
        {
            s->dump(d + 1);
        }
    }

    llvm::Value* accept(Visitor& v) override
    {
        return v.visit_block(*this);
    }
};
