#pragma once
#include "../lexer_context.h"

// Between tokens: skips whitespace, emits one-byte tokens, picks the next state.
class StartState : public LexerState
{
public:
    bool handle(LexerContext& ctx, bool eof, unsigned char b) override
    {
        if (eof)
        {
            ctx.finished = true;
            return true;
        }

        if (b == ' ' || b == '\t' || b == '\r')
        {
            return true;
        }
        if (b == '\n')
        {
            ctx.endLine();
            return true;
        }

        // identifier / keyword: ASCII letter, '_' or the lead byte of a Cyrillic letter.
        // The byte is not consumed: IdentState handles it as the first byte of the word.
        if (lexchar::isAsciiLetter(b) || b == '_' || lexchar::isCyrillicLead(b))
        {
            ctx.markStart();
            ctx.changeState(StateId::Ident);
            return false;
        }
        if (lexchar::isDigit(b))
        {
            ctx.markStart();
            ctx.changeState(StateId::Number);
            return false;
        }

        if (b == '{' || b == '}')
        {
            ctx.emit("block", std::string(1, (char)b), ctx.line, ctx.col);
            return true;
        }
        if (b == '(' || b == ')')
        {
            ctx.emit("paren", std::string(1, (char)b), ctx.line, ctx.col);
            return true;
        }
        if (b == ',') { ctx.emit("comma", ",", ctx.line, ctx.col); return true; }
        if (b == '.') { ctx.emit("dot",   ".", ctx.line, ctx.col); return true; }

        if (b == '+' || b == '*')
        {
            ctx.emit("operator", std::string(1, (char)b), ctx.line, ctx.col);
            return true;
        }

        // operators that need a second byte
        if (b == ':') { ctx.markStart(); ctx.changeState(StateId::Assign);   return true; }
        if (b == '-') { ctx.markStart(); ctx.changeState(StateId::Minus);    return true; }
        if (b == '=') { ctx.markStart(); ctx.changeState(StateId::Equals);   return true; }
        if (b == '!') { ctx.markStart(); ctx.changeState(StateId::Negation); return true; }
        if (b == '#') { ctx.changeState(StateId::Comment);   return true; }
        if (b == '"') { ctx.markStart(); ctx.changeState(StateId::String);   return true; }

        ctx.fail("unexpected byte " + LexerContext::describeByte(b), ctx.line, ctx.col);
    }
};
