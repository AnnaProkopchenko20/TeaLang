#pragma once
#include <map>
#include <string>

// The ONLY place where the keywords of the language are spelled.
// The lexer builds its keyword table from here and the AST dump prints from here.
namespace kw
{
    inline constexpr const char* I32    = "горнятко_32чл";  // i32
    inline constexpr const char* I64    = "кухоль_64чл";    // i64
    inline constexpr const char* BOOL   = "заповненість";   // bool
    inline constexpr const char* MUT    = "відкрита";       // mut
    inline constexpr const char* TRUE_  = "повне";          // true
    inline constexpr const char* FALSE_ = "пусте";          // false
    inline constexpr const char* IF     = "якщо";           // if
    inline constexpr const char* ELSE   = "інакше";         // else
    inline constexpr const char* WHILE  = "допоки";         // while
    inline constexpr const char* EXIT   = "чайна_перерва";  // exit

    inline constexpr const char* FACT   = "факт";
    inline constexpr const char* FN     = "fn";
    inline constexpr const char* STRUCT = "struct";
    inline constexpr const char* PRINT  = "print";
    inline constexpr const char* READ   = "read";

    // the language has no keyword for "not mutable", so the dump uses a plain flag
    inline constexpr const char* IMMUTABLE_FLAG = "const";

    // word -> token kind, used by the lexer
    inline std::map<std::string, std::string> keyword_table()
    {
        return {
            {I32, "keyword"}, {I64, "keyword"}, {BOOL, "keyword"}, {MUT, "keyword"},
            {TRUE_, "keyword"}, {FALSE_, "keyword"}, {IF, "keyword"}, {ELSE, "keyword"},
            {WHILE, "keyword"}, {EXIT, "keyword"},
            {FACT, "keyword"}, {FN, "keyword"}, {STRUCT, "keyword"}, {PRINT, "keyword"}, {READ, "keyword"}
        };
    }

    // Accepts the internal names ("i32", "i64", "bool") as well as the keywords
    // themselves, and always returns the keyword.
    inline std::string type_keyword(const std::string& t)
    {
        if (t == "i32")  { return I32; }
        if (t == "i64")  { return I64; }
        if (t == "bool") { return BOOL; }
        return t;
    }
}
