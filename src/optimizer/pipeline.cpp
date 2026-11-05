#include "optimizer/pipeline.h"

#include "optimizer/const_inliner.h"
#include "optimizer/effects.h"
#include "optimizer/fn_remover.h"
#include "optimizer/fn_use_counter.h"
#include "optimizer/inliner.h"
#include "optimizer/unreachable.h"
#include "optimizer/var_remover.h"
#include "optimizer/var_use_counter.h"

OptStats optimize(ProgramNode& program)
{
    OptStats stats;
    int inline_counter = 0;

    while (true)
    {
        ++stats.rounds;

        // 1. unreachable code
        int unreachable = 0;
        {
            UnreachableStripper s;
            program.accept(s);
            unreachable = s.removed;
        }

        // 2. constants
        int constants = inline_constants(program);

        // 3. effects (depend on the current tree: recomputed every round)
        EffectOracle oracle;
        oracle.analyse(program);

        // 4. unused variables (fresh counter every round)
        int vars = 0;
        {
            VariableUseCounter counter(oracle);
            program.accept(counter);
            auto dead = counter.dead();
            if (!dead.empty())
            {
                UnusedVariableRemover remover(dead);
                program.accept(remover);
                vars = remover.removed;
            }
        }

        // 5. unused functions, counted on the tree AFTER the variables went
        int fns = 0;
        {
            FunctionUseCounter counter;
            program.accept(counter);
            UnusedFunctionRemover remover(counter.calls);
            program.accept(remover);
            fns = remover.removed;
        }

        // 6. inline one call
        int inl = 0;
        {
            EffectOracle fresh;
            fresh.analyse(program);
            FunctionUseCounter counter;
            program.accept(counter);
            inl = inline_one(program, counter.calls, fresh, inline_counter);
        }

        stats.unreachable += unreachable;
        stats.constants   += constants;
        stats.variables   += vars;
        stats.functions   += fns;
        stats.inlined     += inl;

        if (unreachable + constants + vars + fns + inl == 0) break;
    }
    return stats;
}
