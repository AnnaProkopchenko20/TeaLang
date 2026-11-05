#include "optimizer/const_inliner.h"

#include <map>
#include <string>

#include "optimizer/walk_visitor.h"

namespace
{
    bool is_bool_type(const std::string& t) { return t == "bool" || t == kw::BOOL; }

    std::unique_ptr<ExprNode> literal(const VarNameNode& v, const GlobalDeclNode& g)
    {
        if (is_bool_type(g.type_name))
        {
            auto b = std::make_unique<BoolNode>(v.line, v.col, g.folded_bool);
            b->type = kw::BOOL;
            return b;
        }
        auto n = std::make_unique<NumberNode>(v.line, v.col, std::to_string(g.folded_int));
        n->value = g.folded_int;
        n->type = v.type;
        return n;
    }

    struct Replacer
    {
        int replaced = 0;

        void expr(std::unique_ptr<ExprNode>& slot)
        {
            if (!slot) return;
            ExprNode* e = slot.get();
            if (auto* v = dynamic_cast<VarNameNode*>(e))
            {
                if (auto* g = dynamic_cast<GlobalDeclNode*>(v->decl))
                {
                    std::unique_ptr<ExprNode> lit = literal(*v, *g);
                    slot = std::move(lit);
                    ++replaced;
                }
                return;
            }
            if (auto* b = dynamic_cast<BinaryOpNode*>(e)) { expr(b->left); expr(b->right); return; }
            if (auto* n = dynamic_cast<NotNode*>(e))      { expr(n->expr); return; }
            if (auto* f = dynamic_cast<FieldAccessNode*>(e)) { expr(f->object); return; }
            if (auto* c = dynamic_cast<FunctionCallNode*>(e)) { for (auto& a : c->args) expr(a); return; }
            if (auto* m = dynamic_cast<MemberCallNode*>(e))
            {
                expr(m->object);
                for (auto& a : m->args) expr(a);
                return;
            }
            if (auto* i = dynamic_cast<InitNode*>(e)) { for (auto& it : i->items) expr(it); return; }
        }

        void block(BlockNode* b)
        {
            if (!b) return;
            for (auto& s : b->stmts) if (s) stmt(*s);
        }

        void stmt(StmtNode& s)
        {
            if (auto* d = dynamic_cast<DeclNode*>(&s))
            {
                if (d->expr) for (auto& it : d->expr->items) expr(it);
            }
            else if (auto* a = dynamic_cast<AsmtNode*>(&s))  { expr(a->rhs); }
            else if (auto* x = dynamic_cast<ExitNode*>(&s))  { expr(x->expr); }
            else if (auto* p = dynamic_cast<PrintNode*>(&s)) { if (!p->is_string) expr(p->expr); }
            else if (auto* i = dynamic_cast<IfNode*>(&s))
            {
                expr(i->cond);
                block(i->then_block.get());
                block(i->else_block.get());
            }
            else if (auto* w = dynamic_cast<WhileNode*>(&s)) { expr(w->cond); block(w->body.get()); }
            else if (auto* f = dynamic_cast<ForNode*>(&s))
            {
                if (f->init) stmt(*f->init);
                expr(f->cond);
                if (f->step) stmt(*f->step);
                block(f->body.get());
            }
            else if (auto* sp = dynamic_cast<SpawnNode*>(&s))
            {
                if (sp->call) for (auto& a : sp->call->args) expr(a);
            }
        }

        void program(ProgramNode& p)
        {
            for (auto& f : p.functions) if (f) block(f->body.get());
            for (auto& s : p.structs)
                if (s) for (auto& m : s->member_fns) if (m) block(m->body.get());
            for (auto& s : p.stmts) if (s) stmt(*s);
        }
    };

    struct ConstReadCounter : WalkVisitor
    {
        std::map<const DeclNode*, int> reads;
        llvm::Value* visit_var(VarNameNode& n) override
        {
            if (dynamic_cast<GlobalDeclNode*>(n.decl)) reads[n.decl] += 1;
            return nullptr;
        }
    };
}

int inline_constants(ProgramNode& program)
{
    Replacer r;
    r.program(program);

    ConstReadCounter counter;
    program.accept(counter);

    int removed = 0;
    std::vector<std::unique_ptr<GlobalDeclNode>> keep;
    for (auto& g : program.globals)
    {
        auto it = counter.reads.find(g.get());
        if (it != counter.reads.end() && it->second > 0) keep.push_back(std::move(g));
        else ++removed;
    }
    program.globals = std::move(keep);
    return r.replaced + removed;
}
