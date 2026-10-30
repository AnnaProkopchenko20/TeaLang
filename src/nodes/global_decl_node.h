#pragma once
#include <string>
#include <memory>
#include <iostream>
#include <cstdint>
#include "decl_node.h"
#include "expr_node.h"
#include "../visitor.h"
#include "../keywords.h"

struct GlobalDeclNode : DeclNode {
    std::unique_ptr<ExprNode> const_expr;
    int64_t folded_int = 0;
    bool    folded_bool = false;

    GlobalDeclNode(int l, int c, std::string t, std::string n, std::unique_ptr<ExprNode> e)
        : DeclNode(l, c, std::move(t), std::move(n), false, std::unique_ptr<InitNode>(nullptr))
        , const_expr(std::move(e)) {}

    void dump(int d) const override {
        indent(d);
        std::cout << "GlobalDecl " << name << " " << kw::type_keyword(type_name)
                  << " " << kw::IMMUTABLE_FLAG << "\n";
        if (const_expr) const_expr->dump(d + 1);
    }

    llvm::Value* accept(Visitor& v) override {
        return v.visit_global_decl(*this);
    }
};
