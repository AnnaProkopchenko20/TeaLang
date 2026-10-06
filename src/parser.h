#pragma once
#include <stdexcept>
#include <string>
#include <vector>

#include "ast.h"
#include "keywords.h"
#include "lexer.h"

// Recursive-descent parser: one method per grammar rule.
// The lexer gives tokens grouped by line; the end of a line is the NL of the grammar.
//
// program     ::= { statement }
// statement   ::= declaration | assignment | if_stmt | while_stmt | exit_stmt
// declaration ::= [ "відкрита" ] type identifier ":=" expression NL
// assignment  ::= expression "=>" identifier NL
// if_stmt     ::= "якщо" "(" expression ")" NL block [ "інакше" NL block ]
// while_stmt  ::= "допоки" "(" expression ")" NL block
// block       ::= "{" NL statement { statement } "}" NL
// exit_stmt   ::= "чайна_перерва" factor NL
// type        ::= "горнятко_32чл" | "кухоль_64чл" | "заповненість"
// expression  ::= arith [ ( "==" | "!=" ) arith ]
// arith       ::= term { ( "+" | "-" ) term }
// term        ::= factor { "*" factor }
// factor      ::= number | "повне" | "пусте" | identifier | "!" factor
//
// Errors are thrown as std::runtime_error("line L:C: message").
class Parser
{
public:
    explicit Parser(const std::vector<std::vector<Token>>& lines);

    std::unique_ptr<ProgramNode> parse_program();

private:
    const std::vector<std::vector<Token>>& lines;
    std::vector<Token> toks;   // the current line
    size_t pos = 0;            // token cursor inside the current line
    size_t next_idx = 0;       // index of the first line not consumed yet

    // ---- line cursor (empty lines are skipped) ----
    const std::vector<Token>* peek_line() const;
    void next_line();          // call only if peek_line() != nullptr

    // ---- token cursor ----
    const Token* peek() const;
    Token eat();
    bool at(const std::string& kind, const std::string& text = "") const;
    bool at_type() const;
    bool at_expression_start() const;

    // ---- errors ----
    static std::string where(int line, int col);
    std::runtime_error error_at(const Token& t, const std::string& msg) const;
    std::runtime_error error(const std::string& msg) const;
    Token expect(const std::string& kind, const std::string& text, const std::string& what);
    void end_of_line();

    // ---- grammar rules ----
    std::unique_ptr<StmtNode> parse_statement();
    std::unique_ptr<StmtNode> parse_decl();
    std::unique_ptr<StmtNode> parse_assign();
    std::unique_ptr<StmtNode> parse_exit();
    std::unique_ptr<IfNode> parse_if();
    std::unique_ptr<WhileNode> parse_while();
    std::unique_ptr<BlockNode> parse_block(const std::string& after);
    std::unique_ptr<ExprNode> parse_condition(const std::string& after);
    std::unique_ptr<ExprNode> parse_expr();
    std::unique_ptr<ExprNode> parse_arith();
    std::unique_ptr<ExprNode> parse_term();
    std::unique_ptr<ExprNode> parse_factor();
};
