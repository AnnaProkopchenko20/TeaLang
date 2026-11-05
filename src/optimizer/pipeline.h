#pragma once
#include "ast.h"

struct OptStats
{
    int rounds      = 0;
    int unreachable = 0;   // statements after an exit
    int constants   = 0;   // replaced reads + removed constants
    int variables   = 0;   // declarations and assignments
    int functions   = 0;   // removed global functions
    int inlined     = 0;   // inlined calls
};

// Runs on a CHECKED tree until a round changes nothing. Order inside a round:
//   1. strip unreachable code        4. remove unused variables
//   2. inline constants              5. remove unused functions
//   3. recompute effects             6. inline one call
OptStats optimize(ProgramNode& program);
