#pragma once
#include "value_node.h"

struct NumberNode : ValueNode {
    string text;          // digits exactly as written; the checker decides i32 / i64 / too big
    long long value = 0;  // filled in by the SemanticChecker
    NumberNode(int l, int c, string t) : ValueNode(l, c), text(std::move(t)) {}

    void dump(int d) const override
    {
        indent(d); std::cout << "Const " << text << "\n";
    }

    llvm::Value* accept(Visitor& v) override
    {
        return v.visit_number(*this);
    }
};
