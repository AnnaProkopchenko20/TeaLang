#pragma once
#include "node.h"
#include "stmt_node.h"
#include "global_decl_node.h"
#include "struct_decl_node.h"
#include "fn_decl_node.h"

struct ProgramNode : Node {
    vector<unique_ptr<GlobalDeclNode>> globals;
    vector<unique_ptr<StructDeclNode>> structs;
    vector<unique_ptr<FunctionDeclNode>> functions;
    vector<unique_ptr<StmtNode>> stmts;

    ProgramNode(int l, int c,
                vector<unique_ptr<GlobalDeclNode>> g_decls,
                vector<unique_ptr<StructDeclNode>> s_decls,
                vector<unique_ptr<FunctionDeclNode>> f_decls,
                vector<unique_ptr<StmtNode>> s)
        : Node(l, c), globals(std::move(g_decls)), structs(std::move(s_decls)), functions(std::move(f_decls)), stmts(std::move(s)) {}

    ProgramNode(int l, int c,
                vector<unique_ptr<StructDeclNode>> s_decls,
                vector<unique_ptr<FunctionDeclNode>> f_decls,
                vector<unique_ptr<StmtNode>> s)
        : Node(l, c), structs(std::move(s_decls)), functions(std::move(f_decls)), stmts(std::move(s)) {}

    ProgramNode(int l, int c, vector<unique_ptr<StmtNode>> s)
        : Node(l, c), stmts(std::move(s)) {}

    void dump(int d) const override
    {
        indent(d); std::cout << "Program\n";
        for (auto& g : globals) { g->dump(d + 1); }
        for (auto& st : structs) { st->dump(d + 1); }
        for (auto& fn : functions) { fn->dump(d + 1); }
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
