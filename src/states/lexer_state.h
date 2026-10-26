#pragma once
#include <memory>

class LexerContext;

enum class StateId { Start, Ident, Number, Assign, Equals, Negation, Minus, String };

// Base class of all lexer states (the "State" of the State pattern).
class LexerState
{
public:
    virtual ~LexerState() = default;

    // Handles one byte. `eof` is true after the last byte (then b == 0).
    // Returns true  -> the byte is consumed, move on to the next one;
    //         false -> the byte is NOT consumed and is handled again
    //                  by the state that the context switched to.
    virtual bool handle(LexerContext& ctx, bool eof, unsigned char b) = 0;
};

// Creates a state by id. Declared here, defined in lexer_states.h,
// so that concrete states do not have to include each other.
inline std::unique_ptr<LexerState> makeState(StateId id);
