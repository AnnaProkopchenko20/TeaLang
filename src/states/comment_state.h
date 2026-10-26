#pragma once
#include "../lexer_context.h"

// Discards all characters from '#' until newline or EOF.
// The newline is not consumed, so StartState can process it as NL.
class CommentState : public LexerState
{
public:
    bool handle(LexerContext& ctx, bool eof, unsigned char b) override
    {
        if (eof)
        {
            ctx.finished = true;
            return true;
        }
        if (b == '\n')
        {
            ctx.changeState(StateId::Start);
            return false;
        }
        return true;
    }
};
