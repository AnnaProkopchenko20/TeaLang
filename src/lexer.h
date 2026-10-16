#pragma once
#include <string>
#include <vector>

struct Token
{
    std::string kind;   // keyword, identifier, number, operator, block, paren
    std::string text;
    int line;
    int col;            // column counted in characters (a Cyrillic letter = 1)
};

// Returns tokens grouped by source line (the end of a line is the NL of the grammar).
// Throws std::runtime_error("line L:C: message") on a lexical error.
std::vector<std::vector<Token>> lex(const std::string& data);
