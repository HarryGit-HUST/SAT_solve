#pragma once
#include "types.h"
#include "config.h"
#include "timer.h"

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
    SolverStats stats; // 求解过程统计（决策/传播/冲突次数）
};

Solver2WL *create_solver_2wl(const CNFFormula *formula);
void free_solver_2wl(Solver2WL *solver);

// 2WL 优化版 DPLL（带超时控制）
SolverStatus dpll_solve_optimized_timeout(Solver2WL *solver, double timeout_ms, double *elapsed_ms);