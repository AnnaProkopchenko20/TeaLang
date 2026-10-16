#pragma once
#include "../lexer_context.h"

// Identifier or keyword. A Cyrillic letter is 2 UTF-8 bytes, so after a
// lead byte (0xD0..0xD3) exactly one continuation byte (0x80..0xBF) must follow.
class IdentState : public LexerState
{
public:
    bool handle(LexerContext& ctx, bool eof, unsigned char b) override
    {
        if (eof)
        {
            if (expectContinuation)
            {
                ctx.fail("truncated UTF-8 sequence in identifier", ctx.line, ctx.col);
            }
            return finishWord(ctx);
        }

        if (expectContinuation)
        {
            if (lexchar::isContinuation(b))
            {
                expectContinuation = false;
                return true;
            }
            ctx.fail("invalid UTF-8 sequence in identifier", ctx.line, ctx.col);
        }

        if (lexchar::isAsciiLetter(b) || lexchar::isDigit(b) || b == '_')
        {
            return true;
        }
        if (lexchar::isCyrillicLead(b))
        {
            expectContinuation = true;
            return true;
        }
        if (b >= 0x80)
        {
            ctx.fail("character outside the Cyrillic range in identifier", ctx.line, ctx.col);
        }

        return finishWord(ctx);   // any other ASCII byte ends the word
    }

private:
    bool expectContinuation = false;

    bool finishWord(LexerContext& ctx)
    {
        ctx.emitLexeme(ctx.classify(ctx.lexeme()));
        ctx.changeState(StateId::Start);
        return false;             // re-handle the terminating byte in StartState
    }
};
