#pragma once
#include <string>
#include <vector>
#include <memory>
#include <iostream>
#include "expr_node.h"
#include "stmt_node.h"
#include "var_name_node.h"
#include "field_access_node.h"
#include "this_node.h"

inline std::string lhs_chain_string(const ExprNode* e)
{
    if (auto* v = dynamic_cast<const VarNameNode*>(e)) return v->name;
    if (dynamic_cast<const ThisNode*>(e)) return "this";
    if (auto* fa = dynamic_cast<const FieldAccessNode*>(e))
        return lhs_chain_string(fa->object.get()) + "." + fa->field_name;
    return "?";
}

struct AsmtNode : StmtNode {
    std::unique_ptr<ExprNode> lhs;
    std::unique_ptr<ExprNode> rhs;
    DeclNode* decl = nullptr;   // set by the SemanticChecker

    AsmtNode(int l, int c, std::string n, std::unique_ptr<ExprNode> e)
        : StmtNode(l, c), lhs(std::make_unique<VarNameNode>(l, c, std::move(n))), rhs(std::move(e)) {}

    AsmtNode(int l, int c, std::unique_ptr<ExprNode> target, std::unique_ptr<ExprNode> e)
        : StmtNode(l, c), lhs(std::move(target)), rhs(std::move(e)) {}

    void dump(int d) const override
    {
        indent(d);
        std::cout << "Assign " << lhs_chain_string(lhs.get()) << "\n";
        rhs->dump(d + 1);
    }

    llvm::Value* accept(Visitor& v) override
    {
        return v.visit_asmt(*this);
    }
};
