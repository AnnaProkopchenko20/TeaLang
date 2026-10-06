#include "lexer.h"
#include "lexer_context.h"
#include "states/lexer_states.h"

std::vector<std::vector<Token>> lex(const std::string& data)
{
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

    return ctx.takeLines();
}
