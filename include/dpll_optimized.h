#pragma once

#include "types.h"
#include "cnf_parser.h"

struct Watcher
{
    int clause_idx;
    int blocker;
};

struct WatchList
{
    Watcher *list;
    int size;
    int capacity;
};

struct Solver2WL
{
    int num_vars;
    int num_clauses;
    Clause *clauses;
    WatchList *watches;
    int *assignment;
    int *trail;
    int trail_top;
    double *jw_scores;
};

Solver2WL *create_solver_2wl(const CNFFormula *formula);
void free_solver_2wl(Solver2WL *solver);
bool dpll_solve_optimized(Solver2WL *solver);