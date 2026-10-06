#pragma once
#include "../keywords.h"
#include "value_node.h"

struct BoolNode : ValueNode {   // the literals повне / пусте
    bool value;
    BoolNode(int l, int c, bool v) : ValueNode(l, c), value(v) {}

    void dump(int d) const override
    {
        indent(d); std::cout << "Bool " << (value ? kw::TRUE_ : kw::FALSE_) << "\n";
    }

    llvm::Value* accept(Visitor& v) override
    {
        return v.visit_bool(*this);
    }
};
