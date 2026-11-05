#include "optimizer/effects.h"

#include <map>
#include <vector>

#include "optimizer/walk_visitor.h"

namespace
{
    struct GraphBuilder : WalkVisitor
    {
        FunctionDeclNode* current = nullptr;
        std::set<const FunctionDeclNode*> nodes;
        std::set<const FunctionDeclNode*> direct;
        std::map<const FunctionDeclNode*, std::set<const FunctionDeclNode*>> callees;

        void enter(FunctionDeclNode& n)
        {
            FunctionDeclNode* prev = current;
            current = &n;
            nodes.insert(&n);
            if (n.body) n.body->accept(*this);
            current = prev;
        }

        void mark_direct() { if (current) direct.insert(current); }

        void edge(const FunctionDeclNode* callee)
        {
            if (!current) return;
            if (!callee) { direct.insert(current); return; }
            callees[current].insert(callee);
        }

        llvm::Value* visit_fn_decl(FunctionDeclNode& n) override { enter(n); return nullptr; }

        llvm::Value* visit_member_fn_decl(MemberFnDeclNode& n) override
        {
            if (n.mutates) direct.insert(&n);
            enter(n);
            return nullptr;
        }

        llvm::Value* visit_while(WhileNode& n) override { mark_direct(); return WalkVisitor::visit_while(n); }
        llvm::Value* visit_for(ForNode& n)     override { mark_direct(); return WalkVisitor::visit_for(n); }
        llvm::Value* visit_clear(ClearNode&) override { mark_direct(); return nullptr; }
        llvm::Value* visit_print(PrintNode& n) override { mark_direct(); return WalkVisitor::visit_print(n); }
        llvm::Value* visit_read(ReadNode& n)   override { mark_direct(); return WalkVisitor::visit_read(n); }
        llvm::Value* visit_spawn(SpawnNode& n) override { mark_direct(); return WalkVisitor::visit_spawn(n); }
        llvm::Value* visit_join(JoinNode& n)   override { mark_direct(); return WalkVisitor::visit_join(n); }

        llvm::Value* visit_fn_call(FunctionCallNode& n) override
        {
            edge(n.fn_decl);   // may be a MemberFnDeclNode (sibling call)
            return WalkVisitor::visit_fn_call(n);
        }

        llvm::Value* visit_member_call(MemberCallNode& n) override
        {
            edge(n.fn);
            return WalkVisitor::visit_member_call(n);
        }
    };

    struct EffectFinder : WalkVisitor
    {
        const EffectOracle& oracle;
        bool found = false;
        explicit EffectFinder(const EffectOracle& o) : oracle(o) {}

        llvm::Value* visit_read(ReadNode&) override { found = true; return nullptr; }

        llvm::Value* visit_fn_call(FunctionCallNode& n) override
        {
            if (oracle.is_effectful(n.fn_decl)) found = true;
            return WalkVisitor::visit_fn_call(n);
        }

        llvm::Value* visit_member_call(MemberCallNode& n) override
        {
            if (oracle.is_effectful(n.fn)) found = true;
            return WalkVisitor::visit_member_call(n);
        }
    };
}

void EffectOracle::analyse(ProgramNode& program)
{
    effectful_.clear();
    cyclic_.clear();

    GraphBuilder g;
    program.accept(g);
    effectful_ = g.direct;

    // a function that can reach itself may never return
    for (const FunctionDeclNode* f : g.nodes)
    {
        std::set<const FunctionDeclNode*> seen;
        std::vector<const FunctionDeclNode*> stack;
        auto start = g.callees.find(f);
        if (start != g.callees.end())
            for (auto* c : start->second) stack.push_back(c);

        bool cycle = false;
        while (!stack.empty() && !cycle)
        {
            const FunctionDeclNode* c = stack.back();
            stack.pop_back();
            if (c == f) { cycle = true; break; }
            if (!seen.insert(c).second) continue;
            auto next = g.callees.find(c);
            if (next != g.callees.end())
                for (auto* d : next->second) stack.push_back(d);
        }
        if (cycle) { cyclic_.insert(f); effectful_.insert(f); }
    }

    // fixpoint: a caller of an effectful function is effectful
    bool changed = true;
    while (changed)
    {
        changed = false;
        for (const auto& entry : g.callees)
        {
            const FunctionDeclNode* f = entry.first;
            if (effectful_.count(f)) continue;
            for (auto* c : entry.second)
            {
                if (effectful_.count(c)) { effectful_.insert(f); changed = true; break; }
            }
        }
    }
}

bool EffectOracle::has_effect(const ExprNode& expr) const
{
    EffectFinder finder(*this);
    const_cast<ExprNode&>(expr).accept(finder);
    return finder.found;
}
