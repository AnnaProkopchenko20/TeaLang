#pragma once
#include <iostream>
#include "stmt_node.h"
#include "../visitor.h"

// clear()
struct ClearNode : StmtNode {
    ClearNode(int l, int c) : StmtNode(l, c) {}
    void dump(int d) const override { indent(d); std::cout << "Clear\n"; }
    llvm::Value* accept(Visitor& v) override { return v.visit_clear(*this); }
};
