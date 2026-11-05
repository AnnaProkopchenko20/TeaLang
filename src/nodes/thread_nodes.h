#pragma once
#include <memory>
#include <string>
#include "decl_node.h"
#include "fn_call_node.h"
#include "stmt_node.h"
#include "../keywords.h"

// потік t
// The handle is a DeclNode of type "потік" (never a candidate for removal: it is
// not a statement of its own kind, only ThreadDeclNode is). It owns one i64 slot
// in codegen (0 = "no thread").
struct ThreadDeclNode : StmtNode {
    std::string name;
    std::unique_ptr<DeclNode> handle;

    ThreadDeclNode(int l, int c, std::string n)
        : StmtNode(l, c), name(std::move(n)),
          handle(std::make_unique<DeclNode>(l, c, std::string(kw::THREAD), name, true,
                                            std::unique_ptr<InitNode>(nullptr))) {}

    void dump(int d) const override { indent(d); std::cout << "Thread " << name << "\n"; }
    llvm::Value* accept(Visitor& v) override { return v.visit_thread_decl(*this); }
};

// витечи f(args) => t
struct SpawnNode : StmtNode {
    std::unique_ptr<FunctionCallNode> call;
    std::string handle_name;
    DeclNode* handle = nullptr;     // set by the SemanticChecker

    SpawnNode(int l, int c, std::unique_ptr<FunctionCallNode> cl, std::string h)
        : StmtNode(l, c), call(std::move(cl)), handle_name(std::move(h)) {}

    void dump(int d) const override
    {
        indent(d); std::cout << "Spawn " << handle_name << "\n";
        if (call) call->dump(d + 1);
    }
    llvm::Value* accept(Visitor& v) override { return v.visit_spawn(*this); }
};

// дочекатись t
struct JoinNode : StmtNode {
    std::string handle_name;
    DeclNode* handle = nullptr;     // set by the SemanticChecker

    JoinNode(int l, int c, std::string h) : StmtNode(l, c), handle_name(std::move(h)) {}

    void dump(int d) const override { indent(d); std::cout << "Join " << handle_name << "\n"; }
    llvm::Value* accept(Visitor& v) override { return v.visit_join(*this); }
};
