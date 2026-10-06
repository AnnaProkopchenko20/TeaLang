#pragma once
#include "../lexer_context.h"

// After '=' - "==" (equality) or "=>" (assignment). A single '=' is not an operator.
class EqualsState : public LexerState
{
public:
    bool handle(LexerContext& ctx, bool eof, unsigned char b) override
    {
        if (!eof && b == '=')
        {
            ctx.emit("operator", "==", ctx.line, ctx.startCol);
            ctx.changeState(StateId::Start);
            return true;
        }
        if (!eof && b == '>')
        {
            ctx.emit("operator", "=>", ctx.line, ctx.startCol);
            ctx.changeState(StateId::Start);
            return true;
        }
        ctx.fail("expected '==' or '=>' (a single '=' is not an operator)", ctx.line, ctx.startCol);
    }
};
