#include "optimizer/inliner.h"

#include <iterator>
#include <string>
#include <vector>

namespace
{
    using ExprSlot = std::unique_ptr<ExprNode>;
    using StmtVec  = std::vector<std::unique_ptr<StmtNode>>;

    bool is_scalar(const std::string& t)
    {
        return t == "i32" || t == "i64" || t == "bool" || t == kw::I32 || t == kw::I64 || t == kw::BOOL;
    }

    bool simple_body(const FunctionDeclNode& f)
    {
        if (!f.body || f.body->stmts.empty()) return false;
        const auto& s = f.body->stmts;
        for (size_t i = 0; i + 1 < s.size(); ++i)
        {
            StmtNode* p = s[i].get();
            if (!(dynamic_cast<DeclNode*>(p) || dynamic_cast<AsmtNode*>(p) || dynamic_cast<PrintNode*>(p)))
                return false;
        }
        return dynamic_cast<ExitNode*>(s.back().get()) != nullptr;
    }

    // ---------------------------------------------------------------- cloning
    struct Cloner
    {
        std::map<const DeclNode*, DeclNode*> remap;
        std::string prefix;
        bool ok = true;

        ExprSlot expr(const ExprNode* e)
        {
            if (!e) { ok = false; return nullptr; }
            ExprSlot out;
            if (auto* n = dynamic_cast<const NumberNode*>(e))
            {
                auto c = std::make_unique<NumberNode>(n->line, n->col, n->text);
                c->value = n->value;
                out = std::move(c);
            }
            else if (auto* b = dynamic_cast<const BoolNode*>(e))
            {
                out = std::make_unique<BoolNode>(b->line, b->col, b->value);
            }
            else if (auto* v = dynamic_cast<const VarNameNode*>(e))
            {
                auto it = remap.find(v->decl);
                auto c = std::make_unique<VarNameNode>(v->line, v->col,
                                                       it != remap.end() ? it->second->name : v->name);
                c->decl = (it != remap.end()) ? it->second : v->decl;
                out = std::move(c);
            }
            else if (auto* bo = dynamic_cast<const BinaryOpNode*>(e))
            {
                auto l = expr(bo->left.get());
                auto r = expr(bo->right.get());
                if (!l || !r) { ok = false; return nullptr; }
                out = std::make_unique<BinaryOpNode>(bo->line, bo->col, bo->op, std::move(l), std::move(r));
            }
            else if (auto* nn = dynamic_cast<const NotNode*>(e))
            {
                auto x = expr(nn->expr.get());
                if (!x) { ok = false; return nullptr; }
                out = std::make_unique<NotNode>(nn->line, nn->col, std::move(x));
            }
            else if (auto* fa = dynamic_cast<const FieldAccessNode*>(e))
            {
                auto o = expr(fa->object.get());
                if (!o) { ok = false; return nullptr; }
                auto c = std::make_unique<FieldAccessNode>(fa->line, fa->col, std::move(o), fa->field_name);
                c->field_index = fa->field_index;
                out = std::move(c);
            }
            else if (auto* fc = dynamic_cast<const FunctionCallNode*>(e))
            {
                std::vector<ExprSlot> args;
                for (auto& a : fc->args)
                {
                    auto x = expr(a.get());
                    if (!x) { ok = false; return nullptr; }
                    args.push_back(std::move(x));
                }
                auto c = std::make_unique<FunctionCallNode>(fc->line, fc->col, fc->name, std::move(args));
                c->fn_decl = fc->fn_decl;
                out = std::move(c);
            }
            else if (auto* mc = dynamic_cast<const MemberCallNode*>(e))
            {
                auto o = expr(mc->object.get());
                if (!o) { ok = false; return nullptr; }
                std::vector<ExprSlot> args;
                for (auto& a : mc->args)
                {
                    auto x = expr(a.get());
                    if (!x) { ok = false; return nullptr; }
                    args.push_back(std::move(x));
                }
                auto c = std::make_unique<MemberCallNode>(mc->line, mc->col, std::move(o), mc->fn_name, std::move(args));
                c->fn = mc->fn;
                out = std::move(c);
            }
            else if (dynamic_cast<const ReadNode*>(e))
            {
                out = std::make_unique<ReadNode>(e->line, e->col);
            }
            else if (auto* in = dynamic_cast<const InitNode*>(e))
            {
                out = init(*in);
                if (!out) { ok = false; return nullptr; }
            }
            else
            {
                ok = false;     // ThisNode or an unknown node: do not inline
                return nullptr;
            }
            out->type = e->type;
            return out;
        }

        std::unique_ptr<InitNode> init(const InitNode& in)
        {
            std::vector<ExprSlot> items;
            for (auto& it : in.items)
            {
                auto c = expr(it.get());
                if (!c) { ok = false; return nullptr; }
                items.push_back(std::move(c));
            }
            auto r = std::make_unique<InitNode>(in.line, in.col, std::move(items));
            r->type = in.type;
            return r;
        }

        std::unique_ptr<StmtNode> stmt(const StmtNode* s)
        {
            if (auto* d = dynamic_cast<const DeclNode*>(s))
            {
                if (!d->expr) { ok = false; return nullptr; }
                auto in = init(*d->expr);
                if (!in) { ok = false; return nullptr; }
                auto nd = std::make_unique<DeclNode>(d->line, d->col, d->type_name, prefix + d->name,
                                                     d->is_mut, std::move(in));
                remap[d] = nd.get();
                return nd;
            }
            if (auto* a = dynamic_cast<const AsmtNode*>(s))
            {
                auto l = expr(a->lhs.get());
                auto r = expr(a->rhs.get());
                if (!l || !r) { ok = false; return nullptr; }
                auto na = std::make_unique<AsmtNode>(a->line, a->col, std::move(l), std::move(r));
                auto it = remap.find(a->decl);
                na->decl = (it != remap.end()) ? it->second : a->decl;
                return na;
            }
            if (auto* p = dynamic_cast<const PrintNode*>(s))
            {
                if (p->is_string) return std::make_unique<PrintNode>(p->line, p->col, p->string_literal);
                auto x = expr(p->expr.get());
                if (!x) { ok = false; return nullptr; }
                return std::make_unique<PrintNode>(p->line, p->col, std::move(x));
            }
            ok = false;
            return nullptr;
        }
    };

    // ------------------------------------------------------------- call sites
    int count_calls(const ExprNode* e)
    {
        if (!e) return 0;
        if (dynamic_cast<const ReadNode*>(e)) return 1;
        if (auto* c = dynamic_cast<const FunctionCallNode*>(e))
        {
            int n = 1;
            for (auto& a : c->args) n += count_calls(a.get());
            return n;
        }
        if (auto* m = dynamic_cast<const MemberCallNode*>(e))
        {
            int n = 1 + count_calls(m->object.get());
            for (auto& a : m->args) n += count_calls(a.get());
            return n;
        }
        if (auto* b = dynamic_cast<const BinaryOpNode*>(e)) return count_calls(b->left.get()) + count_calls(b->right.get());
        if (auto* n = dynamic_cast<const NotNode*>(e)) return count_calls(n->expr.get());
        if (auto* f = dynamic_cast<const FieldAccessNode*>(e)) return count_calls(f->object.get());
        if (auto* i = dynamic_cast<const InitNode*>(e))
        {
            int n = 0;
            for (auto& it : i->items) n += count_calls(it.get());
            return n;
        }
        return 0;
    }

    ExprSlot* find_call(ExprSlot& slot, const FunctionDeclNode* f)
    {
        if (!slot) return nullptr;
        ExprNode* e = slot.get();
        if (auto* c = dynamic_cast<FunctionCallNode*>(e))
        {
            if (c->fn_decl == f) return &slot;
            for (auto& a : c->args) if (auto* r = find_call(a, f)) return r;
            return nullptr;
        }
        if (auto* m = dynamic_cast<MemberCallNode*>(e))
        {
            if (auto* r = find_call(m->object, f)) return r;
            for (auto& a : m->args) if (auto* r = find_call(a, f)) return r;
            return nullptr;
        }
        if (auto* b = dynamic_cast<BinaryOpNode*>(e))
        {
            if (auto* r = find_call(b->left, f)) return r;
            return find_call(b->right, f);
        }
        if (auto* n = dynamic_cast<NotNode*>(e)) return find_call(n->expr, f);
        if (auto* fa = dynamic_cast<FieldAccessNode*>(e)) return find_call(fa->object, f);
        if (auto* i = dynamic_cast<InitNode*>(e))
        {
            for (auto& it : i->items) if (auto* r = find_call(it, f)) return r;
        }
        return nullptr;
    }

    // The expression slots of a statement that may be hoisted over. Returns the
    // number of calls/reads outside those slots that still count (assignment target).
    int collect(StmtNode& s, std::vector<ExprSlot*>& slots)
    {
        if (auto* d = dynamic_cast<DeclNode*>(&s))
        {
            if (d->expr) for (auto& it : d->expr->items) slots.push_back(&it);
            return 0;
        }
        if (auto* a = dynamic_cast<AsmtNode*>(&s)) { slots.push_back(&a->rhs); return count_calls(a->lhs.get()); }
        if (auto* p = dynamic_cast<PrintNode*>(&s)) { if (!p->is_string && p->expr) slots.push_back(&p->expr); return 0; }
        if (auto* x = dynamic_cast<ExitNode*>(&s)) { slots.push_back(&x->expr); return 0; }
        if (auto* i = dynamic_cast<IfNode*>(&s))   { slots.push_back(&i->cond); return 0; }
        return 0;
    }

    void nested(StmtNode& s, std::vector<StmtVec*>& out)
    {
        if (auto* i = dynamic_cast<IfNode*>(&s))
        {
            if (i->then_block) out.push_back(&i->then_block->stmts);
            if (i->else_block) out.push_back(&i->else_block->stmts);
        }
        else if (auto* w = dynamic_cast<WhileNode*>(&s)) { if (w->body) out.push_back(&w->body->stmts); }
        else if (auto* f = dynamic_cast<ForNode*>(&s))   { if (f->body) out.push_back(&f->body->stmts); }
    }

    struct Site
    {
        StmtVec* vec = nullptr;
        size_t idx = 0;
        ExprSlot* slot = nullptr;
    };

    // returns true when the search is finished (found, eligible or not)
    bool find_site(StmtVec& vec, const FunctionDeclNode* f, Site& site)
    {
        for (size_t i = 0; i < vec.size(); ++i)
        {
            StmtNode& s = *vec[i];
            std::vector<ExprSlot*> slots;
            int total = collect(s, slots);
            for (ExprSlot* sl : slots) total += count_calls(sl->get());

            for (ExprSlot* sl : slots)
            {
                if (ExprSlot* hit = find_call(*sl, f))
                {
                    if (total == 1) { site.vec = &vec; site.idx = i; site.slot = hit; }
                    return true;
                }
            }

            std::vector<StmtVec*> kids;
            nested(s, kids);
            for (StmtVec* k : kids) if (find_site(*k, f, site)) return true;
        }
        return false;
    }

    Site search_all(ProgramNode& p, const FunctionDeclNode* f)
    {
        Site site;
        if (find_site(p.stmts, f, site)) return site;
        for (auto& fn : p.functions)
            if (fn && fn->body && find_site(fn->body->stmts, f, site)) return site;
        for (auto& st : p.structs)
            if (st) for (auto& m : st->member_fns)
                if (m && m->body && find_site(m->body->stmts, f, site)) return site;
        return site;
    }

    // ---------------------------------------------------------------- inlining
    bool apply(FunctionDeclNode* F, Site& site, int id)
    {
        std::string prefix = F->name + "." + std::to_string(id) + ".";
        Cloner cl;
        cl.prefix = prefix;

        std::vector<std::unique_ptr<DeclNode>> param_decls;
        for (auto& p : F->params)
        {
            auto d = std::make_unique<DeclNode>(p->line, p->col, p->type_name, prefix + p->name, false,
                                                std::unique_ptr<InitNode>(nullptr));
            cl.remap[p->decl.get()] = d.get();
            param_decls.push_back(std::move(d));
        }

        StmtVec body;
        auto& src = F->body->stmts;
        for (size_t i = 0; i + 1 < src.size(); ++i)
        {
            auto c = cl.stmt(src[i].get());
            if (!cl.ok) return false;
            body.push_back(std::move(c));
        }
        auto* ex = static_cast<ExitNode*>(src.back().get());
        ExprSlot rexpr = cl.expr(ex->expr.get());
        if (!cl.ok || !rexpr) return false;

        auto* call = static_cast<FunctionCallNode*>(site.slot->get());
        if (call->args.size() != param_decls.size()) return false;

        // ---- commit (nothing below can fail) ----
        for (size_t i = 0; i < param_decls.size(); ++i)
        {
            std::string t = call->args[i]->type;
            int l = call->args[i]->line, c = call->args[i]->col;
            std::vector<ExprSlot> items;
            items.push_back(std::move(call->args[i]));
            auto in = std::make_unique<InitNode>(l, c, std::move(items));
            in->type = t;
            param_decls[i]->expr = std::move(in);
        }

        std::string rt = rexpr->type;
        std::vector<ExprSlot> ritems;
        ritems.push_back(std::move(rexpr));
        auto rin = std::make_unique<InitNode>(ex->line, ex->col, std::move(ritems));
        rin->type = rt;
        auto ret = std::make_unique<DeclNode>(ex->line, ex->col, F->return_type, prefix + "ret", false, std::move(rin));

        auto use = std::make_unique<VarNameNode>(call->line, call->col, ret->name);
        use->decl = ret.get();
        use->type = F->return_type;

        StmtVec ins;
        for (auto& d : param_decls) ins.push_back(std::move(d));
        for (auto& s : body) ins.push_back(std::move(s));
        ins.push_back(std::move(ret));

        *site.slot = std::move(use);   // destroys the call node (its arguments were moved out)
        site.vec->insert(site.vec->begin() + (long)site.idx,
                         std::make_move_iterator(ins.begin()), std::make_move_iterator(ins.end()));
        return true;
    }
}

int inline_one(ProgramNode& program,
               const std::map<const FunctionDeclNode*, int>& uses,
               const EffectOracle& oracle,
               int& counter)
{
    for (auto& fp : program.functions)
    {
        FunctionDeclNode* f = fp.get();
        auto it = uses.find(f);
        if (it == uses.end() || it->second != 1) continue;
        if (oracle.is_recursive(f)) continue;
        if (!is_scalar(f->return_type)) continue;

        bool scalar = true;
        for (auto& p : f->params) if (!is_scalar(p->type_name)) scalar = false;
        if (!scalar || !simple_body(*f)) continue;

        Site site = search_all(program, f);
        if (!site.slot) continue;

        if (apply(f, site, counter + 1))
        {
            ++counter;
            return 1;
        }
    }
    return 0;
}
