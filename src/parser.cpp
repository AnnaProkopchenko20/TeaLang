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
    toks = lines[next_idx++];
    pos = 0;
}

// ------------------------------------------------------------ token cursor

const Token* Parser::peek() const
{
    return pos < toks.size() ? &toks[pos] : nullptr;
}

Token Parser::eat()
{
    return toks[pos++];
}

bool Parser::at(const string& kind, const string& text) const
{
    const Token* t = peek();
    return t && t->kind == kind && (text.empty() || t->text == text);
}

bool Parser::at_type() const
{
    return at("keyword", kw::I32) || at("keyword", kw::I64) || at("keyword", kw::BOOL);
}

// FIRST(expression): number | "повне" | "пусте" | identifier | "!"
bool Parser::at_expression_start() const
{
    return at("number") || at("identifier") || at("operator", "!")
        || at("keyword", kw::TRUE_) || at("keyword", kw::FALSE_);
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

// program ::= { statement }
unique_ptr<ProgramNode> Parser::parse_program()
{
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
        stmts.push_back(parse_statement());
    }

    return make_unique<ProgramNode>(first_line, first_col, std::move(stmts));
}

// statement ::= declaration | assignment | if_stmt | while_stmt | exit_stmt
// Consumes one line, or for if / while the whole construct.
unique_ptr<StmtNode> Parser::parse_statement()
{
    if (at("keyword", kw::MUT) || at_type())
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

// declaration ::= [ "відкрита" ] type identifier ":=" expression NL
unique_ptr<StmtNode> Parser::parse_decl()
{
    bool is_mut = at("keyword", kw::MUT);
    if (is_mut) { eat(); }

    if (!at_type())
    {
        throw error("expected a type (" + string(kw::I32) + ", " + kw::I64 + " or " + kw::BOOL + ")");
    }
    Token type = eat();

    Token name = expect("identifier", "", "a variable name");

    if (!at("operator", ":="))
    {
        throw error("expected ':=' and an initialiser after '" + name.text + "'");
    }
    eat();

    auto init = parse_expr();

    return make_unique<DeclNode>(name.line, name.col, type.text, name.text, is_mut, std::move(init));
}

// assignment ::= expression "=>" identifier NL      (the value goes to the right)
unique_ptr<StmtNode> Parser::parse_assign()
{
    auto value = parse_expr();

    expect("operator", "=>", "'=>' after the expression");
    Token name = expect("identifier", "", "a variable name after '=>'");

    return make_unique<AsmtNode>(name.line, name.col, name.text, std::move(value));
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

// factor ::= number | "повне" | "пусте" | identifier | "!" factor
unique_ptr<ExprNode> Parser::parse_factor()
{
    if (at("operator", "!"))
    {
        Token t = eat();
        auto operand = parse_factor();
        return make_unique<NotNode>(t.line, t.col, std::move(operand));
    }
    if (at("identifier"))
    {
        Token t = eat();
        return make_unique<VarNameNode>(t.line, t.col, t.text);
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

    throw error("expected a number, 'повне', 'пусте', a variable or '!'");
}
