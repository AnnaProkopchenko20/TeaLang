#pragma once
#include "expr_node.h"
#include "init_node.h"
#include "../keywords.h"
#include "stmt_node.h"

struct DeclNode : StmtNode {
    string type_name;   // "i32", "i64" or "bool" (the keyword spelling is accepted too)
    string name;
    bool is_mut;
    unique_ptr<InitNode> expr;

    DeclNode(int l, int c, string t, string n, bool m, unique_ptr<InitNode> e)
        : StmtNode(l, c), type_name(std::move(t)), name(std::move(n)), is_mut(m), expr(std::move(e)) {}

    DeclNode(int l, int c, string t, string n, bool m, unique_ptr<ExprNode> e)
        : StmtNode(l, c), type_name(std::move(t)), name(std::move(n)), is_mut(m)
    {
        if (auto* init = dynamic_cast<InitNode*>(e.get()))
        {
            (void)e.release();
            expr.reset(init);
        }
        else
        {
            vector<unique_ptr<ExprNode>> items;
            if (e) items.push_back(std::move(e));
            expr = make_unique<InitNode>(l, c, std::move(items));
        }
    }

    void dump(int d) const override
    {
        indent(d);
        std::cout << "Decl " << name << " " << kw::type_keyword(type_name)
                  << " " << (is_mut ? kw::MUT : kw::IMMUTABLE_FLAG) << "\n";
        if (expr) expr->dump(d + 1);
    }

    llvm::Value* accept(Visitor& v) override
    {
        return v.visit_decl(*this);
    }
};
