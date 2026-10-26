#include "parser.h"

using std::string; using std::vector; using std::unique_ptr; using std::make_unique;

Parser::Parser(const vector<vector<Token>>& l) : lines(l) {}

// ------------------------------------------------------------ line cursor

const vector<Token>* Parser::peek_line() const
{
    size_t j = next_idx;
    while (j < lines.size() && lines[j].empty()) { j++; }
    return j < lines.size() ? &lines[j] : nullptr;
}

void Parser::next_line()
{
    while (next_idx < lines.size() && lines[next_idx].empty()) { next_idx++; }
    if (next_idx < lines.size())
    {
        toks = lines[next_idx++];
    }
    else
    {
        toks.clear();
    }
    pos = 0;
}

// ------------------------------------------------------------ token cursor

const Token* Parser::peek() const
{
    return pos < toks.size() ? &toks[pos] : nullptr;
}

Token Parser::eat()
{
    if (const Token* t = peek())
    {
        pos += 1;
        return *t;
    }
    throw error("unexpected end of line");
}

bool Parser::at(const string& kind, const string& text) const
{
    const Token* t = peek();
    return t && t->kind == kind && (text.empty() || t->text == text);
}

bool Parser::at_primitive_type() const
{
    return at("keyword", kw::I32) || at("keyword", kw::I64) || at("keyword", kw::BOOL);
}

bool Parser::at_type() const
{
    return at_primitive_type() || at("identifier");
}

Token Parser::expect_type()
{
    if (!at_type())
    {
        throw error("expected a type");
    }
    return eat();
}

// FIRST(expression): number | "повне" | "пусте" | identifier | "!" | read
bool Parser::at_expression_start() const
{
    return at("number") || at("identifier") || at("operator", "!")
        || at("keyword", kw::TRUE_) || at("keyword", kw::FALSE_)
        || at("keyword", kw::READ);
}

// ------------------------------------------------------------ errors

string Parser::where(int line, int col)
{
    return "line " + std::to_string(line) + ":" + std::to_string(col) + ": ";
}

std::runtime_error Parser::error_at(const Token& t, const string& msg) const
{
    return std::runtime_error(where(t.line, t.col) + msg);
}

std::runtime_error Parser::error(const string& msg) const
{
    if (const Token* t = peek())
    {
        return std::runtime_error(where(t->line, t->col) + msg + ", got '" + t->text + "'");
    }

    if (toks.empty())
    {
        return std::runtime_error(msg + ", found end of line");
    }

    // columns are counted in characters, a Cyrillic letter is 2 bytes
    const Token& last = toks.back();
    int len = 0;
    for (unsigned char ch : last.text)
    {
        if (ch < 0x80 || ch >= 0xC0) { len++; }
    }
    return std::runtime_error(where(last.line, last.col + len) + msg + ", found end of line");
}

Token Parser::expect(const string& kind, const string& text, const string& what)
{
    if (!at(kind, text)) { throw error("expected " + what); }
    return eat();
}

// NL: the current line must be fully consumed
void Parser::end_of_line()
{
    if (const Token* t = peek())
    {
        throw error_at(*t, "unexpected '" + t->text + "' after the statement");
    }
}

// ------------------------------------------------------------ grammar

enum class ProgramSection { Structs, Functions, Statements };

// program ::= { struct } { fn } { statement }
unique_ptr<ProgramNode> Parser::parse_program()
{
    ProgramSection section = ProgramSection::Structs;
    vector<unique_ptr<StructDeclNode>> structs;
    vector<unique_ptr<FunctionDeclNode>> functions;
    vector<unique_ptr<StmtNode>> stmts;
    int first_line = 1, first_col = 1;

    if (const vector<Token>* f = peek_line())
    {
        first_line = (*f)[0].line;
        first_col = (*f)[0].col;
    }

    while (peek_line())
    {
        next_line();

        if (at("keyword", kw::STRUCT))
        {
            if (section > ProgramSection::Structs)
            {
                throw error_at(toks[0], "struct declarations come before functions");
            }
            structs.push_back(parse_struct_decl());
        }
        else if (at("keyword", kw::FN))
        {
            if (section > ProgramSection::Functions)
            {
                throw error_at(toks[0], "function declarations come before the statements");
            }
            section = ProgramSection::Functions;
            functions.push_back(parse_fn_decl());
        }
        else
        {
            section = ProgramSection::Statements;
            stmts.push_back(parse_statement());
        }
    }

    return make_unique<ProgramNode>(first_line, first_col, std::move(structs), std::move(functions), std::move(stmts));
}

// struct ::= "struct" ident NL "{" NL field_decl { field_decl } { fn } "}" NL
unique_ptr<StructDeclNode> Parser::parse_struct_decl()
{
    Token kw_tok = eat();
    Token name = expect("identifier", "", "a struct name");
    end_of_line();

    const vector<Token>* nl = peek_line();
    if (!nl)
    {
        throw std::runtime_error(
            where(toks.back().line + 1, 1) +
            "expected '{' on its own line after struct '" + name.text + "', found end of file");
    }
    if (!((*nl)[0].kind == "block" && (*nl)[0].text == "{"))
    {
        throw error_at((*nl)[0],
            "expected '{' on its own line after struct '" + name.text + "', got '" + (*nl)[0].text + "'");
    }

    next_line();
    Token open = eat();
    end_of_line();

    vector<unique_ptr<FieldDeclNode>> fields;
    vector<unique_ptr<MemberFnDeclNode>> member_fns;
    bool seen_member_fn = false;

    while (true)
    {
        if (!peek_line())
        {
            throw error_at(open, "'{' is never closed");
        }
        next_line();

        if (at("block", "}"))
        {
            eat();
            end_of_line();
            if (fields.empty())
            {
                throw error_at(open, "a struct must have at least one field");
            }
            auto sdecl = make_unique<StructDeclNode>(
                kw_tok.line, kw_tok.col,
                name.text, std::move(fields), std::move(member_fns));
            for (auto& m : sdecl->member_fns)
            {
                m->struct_decl = sdecl.get();
            }
            return sdecl;
        }

        if (at("keyword", kw::FN))
        {
            if (fields.empty())
            {
                throw error_at(*peek(), "member function before first field");
            }
            seen_member_fn = true;
            member_fns.push_back(parse_member_fn());
            continue;
        }

        if (seen_member_fn)
        {
            throw error_at(*peek(), "field declaration after member function");
        }

        fields.push_back(parse_field_decl());
    }
}

// field_decl ::= type [ "відкрита" ] ident NL
unique_ptr<FieldDeclNode> Parser::parse_field_decl()
{
    Token type_tok = expect_type();
    bool is_mut = at("keyword", kw::MUT);
    if (is_mut) { eat(); }
    Token name_tok = expect("identifier", "", "a field name");
    end_of_line();
    return make_unique<FieldDeclNode>(
        type_tok.line, type_tok.col,
        type_tok.text, name_tok.text, is_mut);
}

vector<unique_ptr<FuncParamNode>> Parser::parse_params()
{
    vector<unique_ptr<FuncParamNode>> params;
    if (!at("paren", ")"))
    {
        while (true)
        {
            params.push_back(parse_param());
            if (at("comma", ","))
            {
                eat();
            }
            else
            {
                break;
            }
        }
    }
    return params;
}

vector<unique_ptr<ExprNode>> Parser::parse_args()
{
    vector<unique_ptr<ExprNode>> args;
    if (!at("paren", ")"))
    {
        while (true)
        {
            args.push_back(parse_expr());
            if (at("comma", ","))
            {
                eat();
            }
            else
            {
                break;
            }
        }
    }
    return args;
}

// fn ::= "fn" ident ":=" "(" [ param { "," param } ] ")" "->" type NL block
unique_ptr<FunctionDeclNode> Parser::parse_fn_decl()
{
    Token kw_tok = eat();
    Token name = expect("identifier", "", "a function name");
    expect("operator", ":=", "':='");
    expect("paren", "(", "'('");
    auto params = parse_params();
    expect("paren", ")", "')'");
    expect("operator", "->", "'->'");
    Token ret_type = expect_type();
    end_of_line();

    auto body = parse_fn_body(name.text, kw_tok);

    return make_unique<FunctionDeclNode>(
        kw_tok.line, kw_tok.col,
        name.text, ret_type.text,
        std::move(params), std::move(body));
}

unique_ptr<MemberFnDeclNode> Parser::parse_member_fn()
{
    Token kw_tok = eat();
    Token name = expect("identifier", "", "a function name");
    expect("operator", ":=", "':='");
    expect("paren", "(", "'('");
    auto params = parse_params();
    expect("paren", ")", "')'");
    expect("operator", "->", "'->'");
    Token ret_type = expect_type();
    end_of_line();

    auto body = parse_fn_body(name.text, kw_tok);

    return make_unique<MemberFnDeclNode>(
        kw_tok.line, kw_tok.col,
        name.text, ret_type.text,
        std::move(params), std::move(body));
}

// param ::= type ident
unique_ptr<FuncParamNode> Parser::parse_param()
{
    Token type_tok = expect_type();
    Token name_tok = expect("identifier", "", "a parameter name");
    return make_unique<FuncParamNode>(
        type_tok.line, type_tok.col,
        type_tok.text, name_tok.text);
}

// function body block: must end with exit, no statement after exit in same block
unique_ptr<BlockNode> Parser::parse_fn_body(const string& name, const Token& kw_tok)
{
    const vector<Token>* nl = peek_line();
    if (!nl)
    {
        throw std::runtime_error(
            where(toks.back().line + 1, 1) +
            "expected '{' on its own line after '" + kw_tok.text + "', found end of file");
    }
    if (!((*nl)[0].kind == "block" && (*nl)[0].text == "{"))
    {
        throw error_at((*nl)[0],
            "expected '{' on its own line after '" + kw_tok.text + "', got '" + (*nl)[0].text + "'");
    }

    next_line();
    Token open = eat();
    end_of_line();

    vector<unique_ptr<StmtNode>> stmts;
    bool seen_exit = false;

    while (true)
    {
        if (!peek_line())
        {
            throw error_at(open, "'{' is never closed");
        }
        next_line();

        if (at("block", "}"))
        {
            eat();
            end_of_line();
            if (stmts.empty() || !seen_exit)
            {
                throw error_at(kw_tok, "function '" + name + "' must end with 'exit'");
            }
            return make_unique<BlockNode>(open.line, open.col, std::move(stmts));
        }

        if (seen_exit)
        {
            throw error_at(toks[0], "statement after 'exit' in the same block");
        }

        if (at("keyword", kw::EXIT))
        {
            seen_exit = true;
            stmts.push_back(parse_exit());
            end_of_line();
        }
        else
        {
            stmts.push_back(parse_statement());
        }
    }
}

// statement ::= declaration | assignment | if_stmt | while_stmt | exit_stmt | print_stmt
unique_ptr<StmtNode> Parser::parse_statement()
{
    if (at("keyword", kw::MUT) || at_primitive_type())
    {
        auto s = parse_decl();
        end_of_line();
        return s;
    }
    if (at("identifier") && pos + 1 < toks.size() && toks[pos + 1].kind == "identifier")
    {
        auto s = parse_decl();
        end_of_line();
        return s;
    }
    if (at("keyword", kw::EXIT))
    {
        auto s = parse_exit();
        end_of_line();
        return s;
    }
    if (at("keyword", kw::PRINT))
    {
        auto s = parse_print();
        end_of_line();
        return s;
    }
    if (at("keyword", kw::IF))    { return parse_if(); }
    if (at("keyword", kw::WHILE)) { return parse_while(); }

    if (at("keyword", kw::ELSE))
    {
        throw error_at(*peek(), "'" + string(kw::ELSE) + "' without an '" + kw::IF + "'");
    }
    if (at("block", "}"))
    {
        throw error_at(*peek(), "'}' without a matching '{'");
    }

    // everything else must be an assignment, which starts with an expression
    if (at_expression_start())
    {
        auto s = parse_assign();
        end_of_line();
        return s;
    }

    throw error_at(*peek(), "cannot start a statement with '" + peek()->text + "'");
}

// print ::= "print" "(" ( string | expr ) ")"
unique_ptr<StmtNode> Parser::parse_print()
{
    Token kw_tok = eat();
    expect("paren", "(", "'('");
    if (at("paren", ")"))
    {
        throw error_at(*peek(), "expected an expression or string literal");
    }

    unique_ptr<StmtNode> node;
    if (at("string"))
    {
        Token str_tok = eat();
        string text = str_tok.text.substr(1, str_tok.text.size() - 2);
        node = make_unique<PrintNode>(kw_tok.line, kw_tok.col, text);
    }
    else
    {
        auto expr = parse_expr();
        node = make_unique<PrintNode>(kw_tok.line, kw_tok.col, std::move(expr));
    }

    expect("paren", ")", "')'");
    return node;
}

// init ::= "{" value { "," value } "}"
// value ::= expression | init
unique_ptr<InitNode> Parser::parse_init()
{
    Token open = expect("block", "{", "'{'");
    vector<unique_ptr<ExprNode>> items;

    while (!at("block", "}"))
    {
        if (at("block", "{"))
        {
            items.push_back(parse_init());
        }
        else
        {
            items.push_back(parse_expr());
        }

        if (at("comma", ","))
        {
            eat();
        }
        else
        {
            break;
        }
    }

    expect("block", "}", "'}'");
    return make_unique<InitNode>(open.line, open.col, std::move(items));
}

// declaration ::= [ "відкрита" ] type identifier ":=" ( expression | init ) NL
unique_ptr<StmtNode> Parser::parse_decl()
{
    bool is_mut = at("keyword", kw::MUT);
    if (is_mut) { eat(); }

    Token type;
    if (at_primitive_type())
    {
        type = eat();
    }
    else if (at("identifier") && pos + 1 < toks.size() && toks[pos + 1].kind == "identifier")
    {
        type = eat();
    }
    else
    {
        throw error("expected a type (" + string(kw::I32) + ", " + kw::I64 + " or " + kw::BOOL + ")");
    }

    Token name = expect("identifier", "", "a variable name");

    if (!at("operator", ":="))
    {
        throw error("expected ':=' and an initialiser after '" + name.text + "'");
    }
    eat();

    unique_ptr<InitNode> init;
    if (at("block", "{"))
    {
        init = parse_init();
    }
    else
    {
        auto expr = parse_expr();
        vector<unique_ptr<ExprNode>> items;
        items.push_back(std::move(expr));
        init = make_unique<InitNode>(name.line, name.col, std::move(items));
    }

    return make_unique<DeclNode>(name.line, name.col, type.text, name.text, is_mut, std::move(init));
}

// assignment ::= expression "=>" target NL      (the value goes to the right)
// target ::= identifier { "." identifier }
unique_ptr<StmtNode> Parser::parse_assign()
{
    auto value = parse_expr();

    expect("operator", "=>", "'=>' after the expression");
    Token name = expect("identifier", "", "a variable name after '=>'");
    unique_ptr<ExprNode> target = make_unique<VarNameNode>(name.line, name.col, name.text);

    while (at("dot", "."))
    {
        Token dot = eat();
        Token f = expect("identifier", "", "a field name");
        target = make_unique<FieldAccessNode>(dot.line, dot.col, std::move(target), f.text);
    }

    return make_unique<AsmtNode>(name.line, name.col, std::move(target), std::move(value));
}

// exit_stmt ::= "чайна_перерва" factor NL
unique_ptr<StmtNode> Parser::parse_exit()
{
    Token k = eat();
    auto value = parse_factor();
    return make_unique<ExitNode>(k.line, k.col, std::move(value));
}

// "(" expression ")"  - shared by if and while; 'after' names the keyword for errors
unique_ptr<ExprNode> Parser::parse_condition(const string& after)
{
    expect("paren", "(", "'(' after '" + after + "'");
    auto cond = parse_expr();
    expect("paren", ")", "')' after the condition");
    return cond;
}

// if_stmt ::= "якщо" "(" expression ")" NL block [ "інакше" NL block ]
unique_ptr<IfNode> Parser::parse_if()
{
    Token k = eat();
    auto cond = parse_condition(kw::IF);
    end_of_line();

    auto then_block = parse_block(kw::IF);
    unique_ptr<BlockNode> else_block;

    const vector<Token>* nl = peek_line();
    if (nl && (*nl)[0].kind == "keyword" && (*nl)[0].text == kw::ELSE)
    {
        next_line();
        eat();                       // 'інакше'
        end_of_line();
        else_block = parse_block(kw::ELSE);
    }

    return make_unique<IfNode>(k.line, k.col, std::move(cond), std::move(then_block), std::move(else_block));
}

// while_stmt ::= "допоки" "(" expression ")" NL block
unique_ptr<WhileNode> Parser::parse_while()
{
    Token k = eat();
    auto cond = parse_condition(kw::WHILE);
    end_of_line();

    auto body = parse_block(kw::WHILE);

    return make_unique<WhileNode>(k.line, k.col, std::move(cond), std::move(body));
}

// block ::= "{" NL statement { statement } "}" NL
// 'after' is the keyword the block belongs to, for the error message.
unique_ptr<BlockNode> Parser::parse_block(const string& after)
{
    const vector<Token>* nl = peek_line();

    if (!nl)
    {
        throw std::runtime_error(where(toks.back().line + 1, 1) + "expected '{' on its own line after '" + after + "', found end of file");
    }
    if (!((*nl)[0].kind == "block" && (*nl)[0].text == "{"))
    {
        throw error_at((*nl)[0], "expected '{' on its own line after '" + after + "', got '" + (*nl)[0].text + "'");
    }

    next_line();
    Token open = eat();
    end_of_line();

    vector<unique_ptr<StmtNode>> stmts;

    while (true)
    {
        if (!peek_line())
        {
            throw error_at(open, "'{' is never closed");
        }

        next_line();

        if (at("block", "}"))
        {
            eat();
            end_of_line();

            if (stmts.empty())
            {
                throw error_at(open, "empty block");
            }
            return make_unique<BlockNode>(open.line, open.col, std::move(stmts));
        }

        stmts.push_back(parse_statement());
    }
}

// expression ::= arith [ ( "==" | "!=" ) arith ]
unique_ptr<ExprNode> Parser::parse_expr()
{
    unique_ptr<ExprNode> node = parse_arith();

    if (at("operator", "==") || at("operator", "!="))
    {
        Token op = eat();
        auto right = parse_arith();
        node = make_unique<BinaryOpNode>(op.line, op.col, op.text, std::move(node), std::move(right));
    }
    return node;
}

// arith ::= term { ( "+" | "-" ) term }
unique_ptr<ExprNode> Parser::parse_arith()
{
    unique_ptr<ExprNode> node = parse_term();

    while (at("operator", "+") || at("operator", "-"))
    {
        Token op = eat();
        auto right = parse_term();
        node = make_unique<BinaryOpNode>(op.line, op.col, op.text, std::move(node), std::move(right));
    }
    return node;
}

// term ::= factor { "*" factor }
unique_ptr<ExprNode> Parser::parse_term()
{
    unique_ptr<ExprNode> node = parse_factor();

    while (at("operator", "*"))
    {
        Token op = eat();
        auto right = parse_factor();
        node = make_unique<BinaryOpNode>(op.line, op.col, op.text, std::move(node), std::move(right));
    }
    return node;
}

unique_ptr<ExprNode> Parser::parse_read()
{
    Token t = eat(); // 'read'
    expect("paren", "(", "'('");
    expect("paren", ")", "')'");
    return make_unique<ReadNode>(t.line, t.col);
}

unique_ptr<ExprNode> Parser::parse_member_call(unique_ptr<ExprNode> left, const Token& name, const Token& dot_tok)
{
    expect("paren", "(", "'('");
    auto args = parse_args();
    expect("paren", ")", "')'");
    return make_unique<MemberCallNode>(dot_tok.line, dot_tok.col, std::move(left), name.text, std::move(args));
}

unique_ptr<ExprNode> Parser::parse_field_access(unique_ptr<ExprNode> left, const Token& name, const Token& dot_tok)
{
    return make_unique<FieldAccessNode>(dot_tok.line, dot_tok.col, std::move(left), name.text);
}

unique_ptr<ExprNode> Parser::parse_identifier_chain()
{
    Token t = eat();
    unique_ptr<ExprNode> left;

    if (at("paren", "("))
    {
        eat();
        auto args = parse_args();
        expect("paren", ")", "')'");
        left = make_unique<FunctionCallNode>(t.line, t.col, t.text, std::move(args));
    }
    else
    {
        left = make_unique<VarNameNode>(t.line, t.col, t.text);
    }

    while (at("dot", "."))
    {
        Token dot_tok = eat();
        Token name = expect("identifier", "", "a field or member function name");
        if (at("paren", "("))
        {
            left = parse_member_call(std::move(left), name, dot_tok);
        }
        else
        {
            left = parse_field_access(std::move(left), name, dot_tok);
        }
    }

    return left;
}

// factor ::= number | "повне" | "пусте" | identifier_chain | "!" factor | read
unique_ptr<ExprNode> Parser::parse_factor()
{
    if (at("operator", "!"))
    {
        Token t = eat();
        auto operand = parse_factor();
        return make_unique<NotNode>(t.line, t.col, std::move(operand));
    }
    if (at("number"))
    {
        Token t = eat();
        return make_unique<NumberNode>(t.line, t.col, t.text);
    }
    if (at("keyword", kw::TRUE_) || at("keyword", kw::FALSE_))
    {
        Token t = eat();
        return make_unique<BoolNode>(t.line, t.col, t.text == kw::TRUE_);
    }
    if (at("keyword", kw::READ))
    {
        return parse_read();
    }
    if (at("identifier"))
    {
        return parse_identifier_chain();
    }

    throw error("expected a number, 'повне', 'пусте', a variable or '!'");
}
