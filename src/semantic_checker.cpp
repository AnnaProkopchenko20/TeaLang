#include "semantic_checker.h"

#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>

#include "keywords.h"

namespace
{
    inline bool is_i32(const std::string& t) { return t == "i32" || t == kw::I32; }
    inline bool is_i64(const std::string& t) { return t == "i64" || t == kw::I64; }
    inline bool is_bool(const std::string& t) { return t == "bool" || t == kw::BOOL; }
    inline bool is_int(const std::string& t) { return is_i32(t) || is_i64(t); }

    inline std::string canonical_type(const std::string& t)
    {
        if (is_i32(t)) return kw::I32;
        if (is_i64(t)) return kw::I64;
        if (is_bool(t)) return kw::BOOL;
        return t;
    }
}

SemanticChecker::SemanticChecker() { scopes.emplace_back(); }

void SemanticChecker::fail(int line, int col, const std::string& msg)
{
    throw std::runtime_error(
        "line " + std::to_string(line) + ":" + std::to_string(col) + ": " + msg);
}

void SemanticChecker::fail(const Node& at, const std::string& msg)
{
    fail(at.line, at.col, msg);
}

bool SemanticChecker::is_builtin_type(const std::string& t)
{
    return is_int(t) || is_bool(t);
}

bool SemanticChecker::is_known_type(const std::string& t)
{
    return is_builtin_type(t) || structs.count(t);
}

void SemanticChecker::check_new_name(const Node& at, const std::string& name)
{
    if (name == kw::READ || name == kw::PRINT)
        fail(at, "'" + name + "' is a reserved name");
    if (is_builtin_type(name))
        fail(at, "'" + name + "' is already declared");
}

void SemanticChecker::check_decl_name(const Node& at, const std::string& name)
{
    check_new_name(at, name);
    if (struct_names.count(name))
        fail(at, "name '" + name + "' is already used by a structure");
    if (function_names.count(name))
        fail(at, "name '" + name + "' is already used by a function");
    if (current_struct_info)
    {
        if (current_struct_info->member_fns.count(name))
            fail(at, "name '" + name + "' is already used by a function");
    }
}

SemanticChecker::ConstValue SemanticChecker::evaluate_const_expr(ExprNode& expr)
{
    if (auto* num = dynamic_cast<NumberNode*>(&expr))
    {
        errno = 0;
        char* endptr = nullptr;
        long long v = std::strtoll(num->text.c_str(), &endptr, 10);
        if (errno == ERANGE)
            fail(*num, "constant " + num->text + " does not fit in " + kw::I64);
        num->type = (v >= INT32_MIN && v <= INT32_MAX) ? kw::I32 : kw::I64;
        num->value = v;
        return {num->type, v, false};
    }

    if (auto* b = dynamic_cast<BoolNode*>(&expr))
    {
        b->type = kw::BOOL;
        return {kw::BOOL, 0, b->value};
    }

    if (auto* v = dynamic_cast<VarNameNode*>(&expr))
    {
        auto it = global_constants.find(v->name);
        if (it == global_constants.end())
            fail(*v, "variable '" + v->name + "' is used before its declaration");
        GlobalDeclNode* gdecl = it->second;
        v->decl = gdecl;
        v->type = gdecl->type_name;
        if (is_bool(gdecl->type_name))
            return {kw::BOOL, 0, gdecl->folded_bool};
        else
            return {canonical_type(gdecl->type_name), gdecl->folded_int, false};
    }

    if (auto* not_node = dynamic_cast<NotNode*>(&expr))
    {
        ConstValue cv = evaluate_const_expr(*not_node->expr);
        if (!is_bool(cv.type))
            fail(*not_node, "cannot apply '!' to " + cv.type);
        not_node->type = kw::BOOL;
        return {kw::BOOL, 0, !cv.bool_val};
    }

    if (auto* bin = dynamic_cast<BinaryOpNode*>(&expr))
    {
        ConstValue left_val = evaluate_const_expr(*bin->left);
        ConstValue right_val = evaluate_const_expr(*bin->right);
        const std::string& lt = left_val.type;
        const std::string& rt = right_val.type;

        if (bin->op == "+" || bin->op == "-" || bin->op == "*")
        {
            if (!is_int(lt)) fail(*bin, "cannot apply '" + bin->op + "' to " + lt);
            if (!is_int(rt)) fail(*bin, "cannot apply '" + bin->op + "' to " + rt);

            if (is_i64(lt) || is_i64(rt))
            {
                bin->type = kw::I64;
                int64_t res = 0;
                bool overflow = false;
                if (bin->op == "+")
                    overflow = __builtin_add_overflow(left_val.int_val, right_val.int_val, &res);
                else if (bin->op == "-")
                    overflow = __builtin_sub_overflow(left_val.int_val, right_val.int_val, &res);
                else if (bin->op == "*")
                    overflow = __builtin_mul_overflow(left_val.int_val, right_val.int_val, &res);

                if (overflow)
                    fail(*bin, "tea overflow: integer overflow in constant expression");
                return {kw::I64, res, false};
            }
            else
            {
                bin->type = kw::I32;
                int32_t res = 0;
                bool overflow = false;
                int32_t l32 = static_cast<int32_t>(left_val.int_val);
                int32_t r32 = static_cast<int32_t>(right_val.int_val);
                if (bin->op == "+")
                    overflow = __builtin_add_overflow(l32, r32, &res);
                else if (bin->op == "-")
                    overflow = __builtin_sub_overflow(l32, r32, &res);
                else if (bin->op == "*")
                    overflow = __builtin_mul_overflow(l32, r32, &res);

                if (overflow)
                    fail(*bin, "tea overflow: integer overflow in constant expression");
                return {kw::I32, res, false};
            }
        }
        else if (bin->op == "==" || bin->op == "!=")
        {
            bool ok = (is_int(lt) && is_int(rt)) || (is_bool(lt) && is_bool(rt));
            if (!ok) fail(*bin, "cannot compare " + lt + " with " + rt);
            bin->type = kw::BOOL;
            bool equal = false;
            if (is_int(lt))
                equal = (left_val.int_val == right_val.int_val);
            else
                equal = (left_val.bool_val == right_val.bool_val);
            bool res = (bin->op == "==") ? equal : !equal;
            return {kw::BOOL, 0, res};
        }
    }

    fail(expr, "not a constant expression");
}

SemanticChecker::LookupResult SemanticChecker::lookup_name(const std::string& name)
{
    for (auto it = scopes.rbegin(); it != scopes.rend(); ++it)
    {
        auto f = it->find(name);
        if (f != it->end())
            return {LookupResult::LocalVar, f->second, -1, f->second->type_name, f->second->is_mut};
    }
    if (current_struct_info)
    {
        auto fit = current_struct_info->field_index.find(name);
        if (fit != current_struct_info->field_index.end())
        {
            const auto& fi = current_struct_info->fields[fit->second];
            return {LookupResult::FieldOfThis, nullptr, fit->second, fi.type, fi.is_mut};
        }
    }
    auto git = global_constants.find(name);
    if (git != global_constants.end())
    {
        return {LookupResult::Constant, git->second, -1, git->second->type_name, false};
    }
    return {LookupResult::NotFound, nullptr, -1, "", false};
}

DeclNode* SemanticChecker::lookup(const Node& at, const std::string& name)
{
    auto res = lookup_name(name);
    if (res.kind == LookupResult::LocalVar || res.kind == LookupResult::Constant)
        return res.decl;
    fail(at, "variable '" + name + "' is used before its declaration");
}

void SemanticChecker::maybe_rewrite_field(std::unique_ptr<ExprNode>& slot)
{
    if (!slot) return;
    auto* v = dynamic_cast<VarNameNode*>(slot.get());
    if (!v) return;
    auto res = lookup_name(v->name);
    if (res.kind != LookupResult::FieldOfThis) return;

    auto this_node = std::make_unique<ThisNode>(v->line, v->col);
    this_node->type = current_struct_info->name;
    auto fa = std::make_unique<FieldAccessNode>(v->line, v->col, std::move(this_node), v->name);
    slot = std::move(fa);
}

void SemanticChecker::check_expr(std::unique_ptr<ExprNode>& slot)
{
    maybe_rewrite_field(slot);
    slot->accept(*this);
}

bool SemanticChecker::is_rooted_at_this(const ExprNode& expr)
{
    if (dynamic_cast<const ThisNode*>(&expr))
        return true;
    if (auto* fa = dynamic_cast<const FieldAccessNode*>(&expr))
        return is_rooted_at_this(*fa->object);
    return false;
}

bool SemanticChecker::is_chain_mut(const ExprNode& expr)
{
    if (dynamic_cast<const ThisNode*>(&expr))
        return true;
    if (auto* v = dynamic_cast<const VarNameNode*>(&expr))
        return v->decl ? v->decl->is_mut : false;
    if (auto* fa = dynamic_cast<const FieldAccessNode*>(&expr))
    {
        if (!is_chain_mut(*fa->object)) return false;
        auto it = structs.find(fa->object->type);
        if (it != structs.end() && fa->field_index >= 0 && fa->field_index < (int)it->second.fields.size())
            return it->second.fields[fa->field_index].is_mut;
        return false;
    }
    return true; // temporary call results are mutable copies
}

bool SemanticChecker::has_const_field_link(const ExprNode& expr)
{
    if (dynamic_cast<const ThisNode*>(&expr))
        return false;
    if (dynamic_cast<const VarNameNode*>(&expr))
        return false;
    if (auto* fa = dynamic_cast<const FieldAccessNode*>(&expr))
    {
        if (has_const_field_link(*fa->object)) return true;
        auto it = structs.find(fa->object->type);
        if (it != structs.end() && fa->field_index >= 0 && fa->field_index < (int)it->second.fields.size())
            return !it->second.fields[fa->field_index].is_mut;
        return false;
    }
    return false;
}

void SemanticChecker::validate_lvalue(ExprNode& expr, const Node& at)
{
    if (auto* v = dynamic_cast<VarNameNode*>(&expr))
    {
        if (!v->decl->is_mut)
            fail(at, "cannot assign to a const variable '" + v->name + "'");
        if (structs.count(v->decl->type_name) && !all_fields_mut(v->decl->type_name))
            fail(at, "cannot assign to struct '" + v->name + "' because some fields are const");
        return;
    }

    if (auto* fa = dynamic_cast<FieldAccessNode*>(&expr))
    {
        std::vector<FieldAccessNode*> chain;
        ExprNode* curr = fa;
        while (auto* f = dynamic_cast<FieldAccessNode*>(curr))
        {
            chain.push_back(f);
            curr = f->object.get();
        }

        bool is_bare_this_field = false;
        if (dynamic_cast<ThisNode*>(curr))
        {
            if (chain.size() == 1)
                is_bare_this_field = true;
        }
        else if (auto* v = dynamic_cast<VarNameNode*>(curr))
        {
            if (!v->decl->is_mut)
                fail(at, "cannot assign to field of const variable '" + v->name + "'");
        }
        else
        {
            fail(at, "cannot assign to non-lvalue");
        }

        for (int i = (int)chain.size() - 1; i >= 1; --i)
        {
            auto* node = chain[i];
            auto it = structs.find(node->object->type);
            if (it != structs.end() && node->field_index >= 0)
            {
                const auto& finfo = it->second.fields[node->field_index];
                if (!finfo.is_mut)
                    fail(at, "cannot write through const field '" + finfo.name + "'");
            }
        }

        auto* leaf = chain.front();
        auto it = structs.find(leaf->object->type);
        if (it != structs.end() && leaf->field_index >= 0)
        {
            const auto& finfo = it->second.fields[leaf->field_index];
            if (!finfo.is_mut)
            {
                if (is_bare_this_field)
                    fail(at, "cannot assign to a const field '" + finfo.name + "'");
                else
                    fail(at, "cannot write through const field '" + finfo.name + "'");
            }
            if (structs.count(finfo.type) && !all_fields_mut(finfo.type))
                fail(at, "cannot assign to struct '" + finfo.name + "' because some fields are const");
        }
        return;
    }

    fail(at, "invalid lvalue");
}

void SemanticChecker::check_assignable(const ExprNode& expr,
    const std::string& want,
    const Node& at,
    const std::string& what)
{
    const std::string& have = expr.type;
    if (is_i32(want) && is_i64(have))
    {
        if (auto* num = dynamic_cast<const NumberNode*>(&expr))
            fail(num->line, num->col, "tea overflow: constant " + num->text + " does not fit in " + kw::type_keyword(want));
        if (auto* init = dynamic_cast<const InitNode*>(&expr))
            if (init->items.size() == 1)
                if (auto* num2 = dynamic_cast<const NumberNode*>(init->items[0].get()))
                    fail(num2->line, num2->col, "tea overflow: constant " + num2->text + " does not fit in " + kw::type_keyword(want));
        fail(at, "tea overflow: cannot narrow " + kw::type_keyword(have) + " to " + kw::type_keyword(want));
    }
    if (canonical_type(have) == canonical_type(want) || (is_i32(have) && is_i64(want))) return;
    fail(at, "cannot " + what + " of type " + kw::type_keyword(want) + " with a value of type " + kw::type_keyword(have));
}

void SemanticChecker::check_condition(std::unique_ptr<ExprNode>& cond, const Node& at, const char* keyword)
{
    check_expr(cond);
    if (!is_bool(cond->type))
        fail(at, std::string("the condition of '") + keyword + "' must be " + kw::BOOL + ", got " + cond->type);
}

bool SemanticChecker::all_fields_mut(const std::string& type_name)
{
    auto it = structs.find(type_name);
    if (it == structs.end()) return true;
    for (const auto& f : it->second.fields)
    {
        if (!f.is_mut) return false;
        if (structs.count(f.type) && !all_fields_mut(f.type)) return false;
    }
    return true;
}

void SemanticChecker::check_init_item(std::unique_ptr<ExprNode>& item, const std::string& expected_type)
{
    if (auto* inner_init = dynamic_cast<InitNode*>(item.get()))
    {
        if (is_builtin_type(expected_type))
            fail(*inner_init, "nested initializer for built-in type '" + expected_type + "'");

        auto it = structs.find(expected_type);
        if (it == structs.end())
            fail(*inner_init, "unknown type '" + expected_type + "'");

        const auto& sinfo = it->second;
        if (inner_init->items.size() != sinfo.fields.size())
            fail(*inner_init, "struct '" + expected_type + "' has " +
                 std::to_string(sinfo.fields.size()) + " field(s), got " +
                 std::to_string(inner_init->items.size()));

        for (size_t i = 0; i < sinfo.fields.size(); ++i)
            check_init_item(inner_init->items[i], sinfo.fields[i].type);

        inner_init->type = expected_type;
        return;
    }

    check_expr(item);
    check_assignable(*item, expected_type, *item, "initialise");
}

llvm::Value* SemanticChecker::visit_struct_decl(StructDeclNode& n)
{
    StructInfo sinfo;
    sinfo.name = n.name;

    for (size_t i = 0; i < n.fields.size(); ++i)
    {
        const auto& f = n.fields[i];
        check_decl_name(*f, f->name);
        if (sinfo.field_index.count(f->name))
            fail(*f, "field '" + f->name + "' is already declared in struct '" + n.name + "'");

        sinfo.field_index[f->name] = (int)i;
        sinfo.fields.push_back({f->type_name, f->name, f->is_mut, (int)i});
    }

    structs[n.name] = std::move(sinfo);
    return nullptr;
}

llvm::Value* SemanticChecker::visit_fn_decl(FunctionDeclNode& n)
{
    current_fn = &n;

    scopes.clear();
    scopes.emplace_back();
    for (const auto& p : n.params)
    {
        scopes.back()[p->name] = p->decl.get();
    }

    n.body->accept(*this);
    scopes.pop_back();

    current_fn = nullptr;
    return nullptr;
}

llvm::Value* SemanticChecker::visit_member_fn_decl(MemberFnDeclNode& n)
{
    current_member_fn = &n;

    scopes.clear();
    scopes.emplace_back();
    for (const auto& p : n.params)
    {
        scopes.back()[p->name] = p->decl.get();
    }

    n.body->accept(*this);
    scopes.pop_back();

    current_member_fn = nullptr;
    return nullptr;
}

llvm::Value* SemanticChecker::visit_program(ProgramNode& n)
{
    // Phase 0: Name pre-pass (Rule C5: constants, structs, top-level functions share one namespace)
    struct TopEntity {
        std::string kind;
        int line;
        int col;
    };
    std::map<std::string, TopEntity> top_level_names;
    struct_names.clear();
    function_names.clear();

    auto register_top_name = [&](const Node& at, const std::string& name, const std::string& kind) {
        check_new_name(at, name);
        auto it = top_level_names.find(name);
        if (it != top_level_names.end()) {
            fail(at, "name '" + name + "' is already declared as a " + it->second.kind);
        }
        top_level_names[name] = {kind, at.line, at.col};
    };

    for (auto& g : n.globals) {
        register_top_name(*g, g->name, "constant");
    }
    for (auto& s : n.structs) {
        register_top_name(*s, s->name, "structure");
        struct_names.insert(s->name);
    }
    for (auto& fn : n.functions) {
        register_top_name(*fn, fn->name, "function");
        function_names.insert(fn->name);
    }

    // Phase 1: Global constants evaluation (in declaration order)
    global_constants.clear();
    for (auto& g : n.globals) {
        ConstValue val = evaluate_const_expr(*g->const_expr);
        const std::string& want = g->type_name;
        const std::string& have = val.type;

        if (is_i32(want)) {
            if (is_bool(have)) {
                fail(*g, "cannot initialise constant of type " + kw::type_keyword(want) + " with a value of type " + kw::type_keyword(have));
            }
            if (is_i64(have)) {
                if (auto* num = dynamic_cast<const NumberNode*>(g->const_expr.get()))
                    fail(num->line, num->col, "tea overflow: constant " + num->text + " does not fit in " + kw::type_keyword(want));
                fail(*g, "tea overflow: cannot narrow " + kw::type_keyword(have) + " to " + kw::type_keyword(want));
            }
            if (val.int_val < INT32_MIN || val.int_val > INT32_MAX) {
                fail(*g, "tea overflow: constant does not fit in " + kw::type_keyword(want));
            }
            g->folded_int = val.int_val;
            g->const_expr->type = kw::I32;
        } else if (is_i64(want)) {
            if (is_bool(have)) {
                fail(*g, "cannot initialise constant of type " + kw::type_keyword(want) + " with a value of type " + kw::type_keyword(have));
            }
            g->folded_int = val.int_val;
            g->const_expr->type = kw::I64;
        } else if (is_bool(want)) {
            if (!is_bool(have)) {
                fail(*g, "cannot initialise constant of type " + kw::type_keyword(want) + " with a value of type " + kw::type_keyword(have));
            }
            g->folded_bool = val.bool_val;
            g->const_expr->type = kw::BOOL;
        }

        global_constants[g->name] = g.get();
    }

    // Phase 2: Register all struct names and fields
    structs.clear();
    for (auto& s : n.structs)
    {
        for (auto& m : s->member_fns)
            m->struct_decl = s.get();
        visit_struct_decl(*s);
    }

    // Phase 3: Register member function headers and check field validity
    for (auto& s : n.structs)
    {
        current_struct_info = &structs[s->name];
        for (const auto& f : s->fields)
        {
            if (!is_known_type(f->type_name))
                fail(*f, "unknown type '" + f->type_name + "'");
            if (f->type_name == s->name)
                fail(*f, "struct '" + s->name + "' cannot contain itself as a field");
        }

        for (auto& m : s->member_fns)
        {
            m->struct_decl = s.get();
            check_new_name(*m, m->name);
            if (current_struct_info->field_index.count(m->name))
                fail(*m, "'" + m->name + "' is already declared in struct '" + s->name + "'");

            if (current_struct_info->member_fns.count(m->name))
                fail(*m, "member function '" + m->name + "' is already declared in struct '" + s->name + "'");

            current_struct_info->member_fns[m->name] = m.get();
        }

        for (auto& m : s->member_fns)
        {
            if (!is_known_type(m->return_type))
                fail(*m, "unknown return type '" + m->return_type + "'");

            std::set<std::string> param_names;
            for (const auto& p : m->params)
            {
                check_decl_name(*p, p->name);
                if (!is_known_type(p->type_name))
                    fail(*p, "unknown type '" + p->type_name + "'");
                if (param_names.count(p->name))
                    fail(*p, "parameter '" + p->name + "' is already declared");
                param_names.insert(p->name);
            }
        }
        current_struct_info = nullptr;
    }

    // Phase 4: Register global functions
    functions.clear();
    for (auto& fn : n.functions)
    {
        if (!is_known_type(fn->return_type))
            fail(*fn, "unknown return type '" + fn->return_type + "'");

        std::set<std::string> param_names;
        for (const auto& p : fn->params)
        {
            check_decl_name(*p, p->name);
            if (!is_known_type(p->type_name))
                fail(*p, "unknown type '" + p->type_name + "'");
            if (param_names.count(p->name))
                fail(*p, "parameter '" + p->name + "' is already declared");
            param_names.insert(p->name);
        }

        functions[fn->name] = fn.get();
    }

    // Phase 5: Check member function bodies
    in_member_body_pass = true;
    for (auto& s : n.structs)
    {
        current_struct_node = s.get();
        current_struct_info = &structs[s->name];
        for (auto& m : s->member_fns)
        {
            current_fn = m.get();
            visit_member_fn_decl(*m);
            current_fn = nullptr;
        }
        current_struct_info = nullptr;
        current_struct_node = nullptr;
    }
    in_member_body_pass = false;

    // Phase 6: Fixed point iteration for mutating member functions
    for (auto* m : directly_mutates)
        m->mutates = true;

    bool changed = true;
    while (changed)
    {
        changed = false;
        for (auto& [caller, callees] : call_edges)
        {
            if (!caller->mutates)
            {
                for (auto* callee : callees)
                {
                    if (callee->mutates)
                    {
                        caller->mutates = true;
                        changed = true;
                        break;
                    }
                }
            }
        }
    }

    // Phase 7: Validate deferred call sites
    for (const auto& call : deferred_calls)
    {
        if (call.target->mutates)
        {
            if (call.has_const_field_link)
                fail(*call.at, "cannot call mutating member function '" + call.target->name + "' through a const field");
            if (!call.chain_is_mut)
                fail(*call.at, "cannot call mutating member function '" + call.target->name + "' on a const object");
        }
    }

    // Phase 8: Check global function bodies
    for (auto& fn : n.functions)
        visit_fn_decl(*fn);

    // Phase 9: Top-level statements
    scopes.clear();
    scopes.emplace_back();
    current_fn = nullptr;
    current_struct_info = nullptr;
    for (auto& s : n.stmts) s->accept(*this);
    scopes.pop_back();

    return nullptr;
}

llvm::Value* SemanticChecker::visit_block(BlockNode& n)
{
    scopes.emplace_back();
    for (auto& s : n.stmts) s->accept(*this);
    scopes.pop_back();
    return nullptr;
}

llvm::Value* SemanticChecker::visit_decl(DeclNode& n)
{
    check_decl_name(n, n.name);
    if (scopes.back().count(n.name))
        fail(n, "variable '" + n.name + "' is already declared in this block");

    if (!is_known_type(n.type_name))
        fail(n, "unknown type '" + n.type_name + "'");

    if (is_builtin_type(n.type_name))
    {
        if (n.expr->items.size() != 1)
            fail(n, "type '" + n.type_name + "' takes 1 initialiser, got " +
                    std::to_string(n.expr->items.size()));

        if (auto* inner_init = dynamic_cast<InitNode*>(n.expr->items[0].get()))
            fail(*inner_init, "nested initializer for built-in type '" + n.type_name + "'");

        check_expr(n.expr->items[0]);
        check_assignable(*n.expr->items[0], n.type_name, n, "initialise '" + n.name + "'");
        n.expr->type = n.expr->items[0]->type;
    }
    else
    {
        const auto& sinfo = structs[n.type_name];
        if (n.expr->items.size() == 1)
        {
            check_expr(n.expr->items[0]);
            if (n.expr->items[0]->type == n.type_name)
            {
                n.expr->type = n.type_name;
                scopes.back()[n.name] = &n;
                return nullptr;
            }
        }

        if (n.expr->items.size() != sinfo.fields.size())
        {
            fail(n, "struct '" + n.type_name + "' has " +
                std::to_string(sinfo.fields.size()) + " field(s), got " +
                std::to_string(n.expr->items.size()));
        }

        for (size_t i = 0; i < sinfo.fields.size(); ++i)
            check_init_item(n.expr->items[i], sinfo.fields[i].type);

        n.expr->type = n.type_name;
    }

    scopes.back()[n.name] = &n;
    return nullptr;
}

llvm::Value* SemanticChecker::visit_asmt(AsmtNode& n)
{
    check_expr(n.lhs);
    if (current_member_fn && is_rooted_at_this(*n.lhs))
        directly_mutates.insert(current_member_fn);

    validate_lvalue(*n.lhs, n);

    if (auto* v = dynamic_cast<VarNameNode*>(n.lhs.get()))
    {
        n.decl = v->decl;
    }

    check_expr(n.rhs);

    std::string what = "assign to '" + lhs_chain_string(n.lhs.get()) + "'";
    if (auto* fa = dynamic_cast<FieldAccessNode*>(n.lhs.get()))
        what = "assign to field '" + fa->field_name + "'";
    if (auto* v = dynamic_cast<VarNameNode*>(n.lhs.get()))
        what = "assign to '" + v->name + "'";

    check_assignable(*n.rhs, n.lhs->type, n, what);
    return nullptr;
}

llvm::Value* SemanticChecker::visit_exit(ExitNode& n)
{
    check_expr(n.expr);
    if (current_fn)
    {
        check_assignable(*n.expr, current_fn->return_type, n, "return");
    }
    else
    {
        if (!is_builtin_type(n.expr->type))
            fail(n, "exit operand must have a built-in type");
    }
    return nullptr;
}

llvm::Value* SemanticChecker::visit_if(IfNode& n)
{
    check_condition(n.cond, n, kw::IF);
    n.then_block->accept(*this);
    if (n.else_block) n.else_block->accept(*this);
    return nullptr;
}

llvm::Value* SemanticChecker::visit_while(WhileNode& n)
{
    check_condition(n.cond, n, kw::WHILE);
    n.body->accept(*this);
    return nullptr;
}

llvm::Value* SemanticChecker::visit_number(NumberNode& n)
{
    errno = 0;
    char* endptr = nullptr;
    long long v = std::strtoll(n.text.c_str(), &endptr, 10);
    if (errno == ERANGE)
        fail(n, "constant " + n.text + " does not fit in " + kw::I64);
    n.type  = (v >= INT32_MIN && v <= INT32_MAX) ? kw::I32 : kw::I64;
    n.value = v;
    return nullptr;
}

llvm::Value* SemanticChecker::visit_bool(BoolNode& n)  { n.type = kw::BOOL; return nullptr; }

llvm::Value* SemanticChecker::visit_this(ThisNode& n)
{
    if (current_struct_info)
        n.type = current_struct_info->name;
    return nullptr;
}

llvm::Value* SemanticChecker::visit_var(VarNameNode& n)
{
    auto res = lookup_name(n.name);
    if (res.kind == LookupResult::NotFound)
        fail(n, "variable '" + n.name + "' is used before its declaration");
    if (res.kind == LookupResult::FieldOfThis)
        fail(n, "internal: VarNameNode for field should have been rewritten");

    n.decl = res.decl;
    n.type = res.decl->type_name;
    return nullptr;
}

llvm::Value* SemanticChecker::visit_not(NotNode& n)
{
    check_expr(n.expr);
    if (!is_bool(n.expr->type))
        fail(n, "cannot apply '!' to " + n.expr->type);
    n.type = kw::BOOL;
    return nullptr;
}

llvm::Value* SemanticChecker::visit_binop(BinaryOpNode& n)
{
    check_expr(n.left);
    check_expr(n.right);
    const std::string& lt = n.left->type;
    const std::string& rt = n.right->type;

    if (n.op == "+" || n.op == "-" || n.op == "*")
    {
        if (!is_int(lt)) fail(n, "cannot apply '" + n.op + "' to " + lt);
        if (!is_int(rt)) fail(n, "cannot apply '" + n.op + "' to " + rt);
        n.type = (is_i64(lt) || is_i64(rt)) ? kw::I64 : kw::I32;
    }
    else
    {
        bool ok = (is_int(lt) && is_int(rt)) || (is_bool(lt) && is_bool(rt));
        if (!ok) fail(n, "cannot compare " + lt + " with " + rt);
        n.type = kw::BOOL;
    }
    return nullptr;
}

llvm::Value* SemanticChecker::visit_fn_call(FunctionCallNode& n)
{
    if (current_struct_info)
    {
        auto mit = current_struct_info->member_fns.find(n.name);
        if (mit != current_struct_info->member_fns.end())
        {
            MemberFnDeclNode* m = mit->second;
            n.fn_decl = m;
            if (n.args.size() != m->params.size())
                fail(n, "'" + n.name + "' takes " + std::to_string(m->params.size()) +
                        " argument(s), got " + std::to_string(n.args.size()));

            for (size_t i = 0; i < n.args.size(); ++i)
            {
                check_expr(n.args[i]);
                const auto& want = m->params[i]->type_name;
                const auto& have = n.args[i]->type;
                if (canonical_type(have) != canonical_type(want) && !(is_i32(have) && is_i64(want)))
                {
                    fail(*n.args[i], "cannot pass argument " + std::to_string(i + 1) + " of '" +
                        n.name + "' of type " + want + " with a value of type " + have);
                }
            }

            n.type = m->return_type;

            if (in_member_body_pass)
            {
                call_edges[current_member_fn].insert(m);
                deferred_calls.push_back({&n, m, true, false});
            }
            return nullptr;
        }
    }

    auto it = functions.find(n.name);
    if (it == functions.end())
        fail(n, "function '" + n.name + "' is not declared");

    FunctionDeclNode* f = it->second;
    n.fn_decl = f;

    if (n.args.size() != f->params.size())
        fail(n, "'" + n.name + "' takes " + std::to_string(f->params.size()) +
                " argument(s), got " + std::to_string(n.args.size()));

    for (size_t i = 0; i < n.args.size(); ++i)
    {
        check_expr(n.args[i]);
        const auto& want = f->params[i]->type_name;
        const auto& have = n.args[i]->type;
        if (canonical_type(have) != canonical_type(want) && !(is_i32(have) && is_i64(want)))
        {
            fail(*n.args[i], "cannot pass argument " + std::to_string(i + 1) + " of '" +
                n.name + "' of type " + want + " with a value of type " + have);
        }
    }

    n.type = f->return_type;
    return nullptr;
}

llvm::Value* SemanticChecker::visit_field_access(FieldAccessNode& n)
{
    check_expr(n.object);
    const auto& cur_type = n.object->type;
    auto it = structs.find(cur_type);
    if (it == structs.end())
        fail(n, "type '" + cur_type + "' is not a struct");

    const auto& sinfo = it->second;
    auto f_it = sinfo.field_index.find(n.field_name);
    if (f_it == sinfo.field_index.end())
        fail(n, "struct '" + cur_type + "' has no field '" + n.field_name + "'");

    n.field_index = f_it->second;
    n.type = sinfo.fields[n.field_index].type;
    return nullptr;
}

llvm::Value* SemanticChecker::visit_member_call(MemberCallNode& n)
{
    check_expr(n.object);
    const auto& cur_type = n.object->type;
    auto it = structs.find(cur_type);
    if (it == structs.end())
        fail(n, "type '" + cur_type + "' is not a struct");

    const auto& sinfo = it->second;
    auto m_it = sinfo.member_fns.find(n.fn_name);
    if (m_it == sinfo.member_fns.end())
        fail(n, "struct '" + cur_type + "' has no member function '" + n.fn_name + "'");

    MemberFnDeclNode* mfn = m_it->second;
    n.fn = mfn;

    if (n.args.size() != mfn->params.size())
        fail(n, "'" + n.fn_name + "' takes " + std::to_string(mfn->params.size()) +
                " argument(s), got " + std::to_string(n.args.size()));

    for (size_t i = 0; i < n.args.size(); ++i)
    {
        check_expr(n.args[i]);
        const auto& want = mfn->params[i]->type_name;
        const auto& have = n.args[i]->type;
        if (canonical_type(have) != canonical_type(want) && !(is_i32(have) && is_i64(want)))
        {
            fail(*n.args[i], "cannot pass argument " + std::to_string(i + 1) + " of '" +
                n.fn_name + "' of type " + want + " with a value of type " + have);
        }
    }

    n.type = mfn->return_type;

    if (in_member_body_pass)
    {
        if (is_rooted_at_this(*n.object))
            call_edges[current_member_fn].insert(mfn);
        deferred_calls.push_back({&n, mfn, is_chain_mut(*n.object), has_const_field_link(*n.object)});
    }
    else
    {
        if (mfn->mutates)
        {
            if (has_const_field_link(*n.object))
                fail(n, "cannot call mutating member function '" + mfn->name + "' through a const field");
            if (!is_chain_mut(*n.object))
                fail(n, "cannot call mutating member function '" + mfn->name + "' on a const object");
        }
    }
    return nullptr;
}

llvm::Value* SemanticChecker::visit_print(PrintNode& n)
{
    if (n.is_string) return nullptr;

    check_expr(n.expr);
    const auto& t = n.expr->type;
    if (!is_builtin_type(t) && !structs.count(t))
        fail(n, "cannot print value of type '" + t + "'");
    return nullptr;
}

llvm::Value* SemanticChecker::visit_read(ReadNode& n)
{
    n.type = kw::I32;
    return nullptr;
}

llvm::Value* SemanticChecker::visit_init(InitNode& n)
{
    for (auto& item : n.items)
        check_expr(item);
    if (!n.items.empty())
        n.type = n.items[0]->type;
    return nullptr;
}

llvm::Value* SemanticChecker::visit_global_decl(GlobalDeclNode&)
{
    return nullptr;
}
