#include "lexer.h"
#include "lexer_context.h"
#include "states/lexer_states.h"

std::vector<std::vector<Token>> lex(const std::string& original)
{
    std::string data = original;

    struct Semi { int line; int col; };
    std::vector<Semi> semis;
    {
        int line = 1, col = 1;
        bool in_string = false, in_comment = false;
        for (size_t k = 0; k < data.size(); ++k)
        {
            unsigned char b = (unsigned char)data[k];
            if (b == '\n') { line++; col = 1; in_string = false; in_comment = false; continue; }
            bool cont = (b >= 0x80 && b <= 0xBF);
            if (!in_comment && !in_string)
            {
                if (b == '#')      { in_comment = true; }
                else if (b == '"') { in_string = true; }
                else if (b == ';') { semis.push_back({line, col}); data[k] = ' '; }
            }
            else if (in_string && b == '"') { in_string = false; }
            if (!cont) col++;
        }
    }

    LexerContext ctx(data);

    while (!ctx.finished)
    {
        bool eof = (ctx.i >= data.size());
        unsigned char b = eof ? 0 : (unsigned char)data[ctx.i];

        bool consumed = ctx.state->handle(ctx, eof, b);
        ctx.applyPendingState();

        if (consumed)
        {
            if (eof) { ctx.finished = true; }
            else     { ctx.advance(); }
        }
    }

    auto lines = ctx.takeLines();

    for (const Semi& s : semis)
    {
        if (lines.size() < (size_t)s.line) lines.resize((size_t)s.line);
        auto& row = lines[(size_t)s.line - 1];
        Token tok{"semicolon", ";", s.line, s.col};
        auto pos = row.begin();
        while (pos != row.end() && pos->col < s.col) ++pos;
        row.insert(pos, tok);
    }
    return lines;
}
