#pragma once
#include "ast.h"

// Replaces every read of a `факт` constant in code and in function bodies by its
// folded literal, then removes the constants nobody reads any more (reads inside
// other constants' initialisers still count, so chains collapse over rounds).
// Returns the number of replaced reads plus removed constants.
int inline_constants(ProgramNode& program);
