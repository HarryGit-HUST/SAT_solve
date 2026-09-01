#include "dpll_optimized.h"
#include <cstdlib>
#include <cmath>
#include <algorithm>
#include <iostream>

static inline int lit_to_idx(int lit)
{
    return (lit > 0) ? (lit << 1) : ((-lit) << 1 | 1);
}

static inline int neg_lit(int lit)
{
    return -lit;
}

static void add_watch(WatchList *wl, int clause_idx, int blocker)
{
    if (wl->size >= wl->capacity)
    {
        wl->capacity = (wl->capacity == 0) ? 4 : (wl->capacity * 2);
        wl->list = (Watcher *)realloc(wl->list, sizeof(Watcher) * wl->capacity);
    }
    wl->list[wl->size].clause_idx = clause_idx;
    wl->list[wl->size].blocker = blocker;
    wl->size++;
}

static inline int eval_lit(const Solver2WL *solver, int lit)
{
    int var = std::abs(lit);
    int val = solver->assignment[var];
    if (val == VAL_UNASSIGNED)
        return VAL_UNASSIGNED;
    return (lit > 0) ? val : (val ^ 1);
}

static inline void assign_literal(Solver2WL *solver, int lit)
{
    int var = std::abs(lit);
    solver->assignment[var] = (lit > 0) ? VAL_TRUE : VAL_FALSE;
    solver->trail[solver->trail_top++] = lit;
}

Solver2WL *create_solver_2wl(const CNFFormula *formula)
{
    Solver2WL *solver = (Solver2WL *)malloc(sizeof(Solver2WL));
    solver->num_vars = formula->num_vars;
    solver->num_clauses = formula->num_clauses;
    solver->clauses = formula->clauses;

    int total_lit_slots = (formula->num_vars + 1) * 2;
    solver->watches = (WatchList *)calloc(total_lit_slots, sizeof(WatchList));
    solver->assignment = (int *)malloc(sizeof(int) * (formula->num_vars + 1));
    for (int i = 0; i <= formula->num_vars; ++i)
        solver->assignment[i] = VAL_UNASSIGNED;

    solver->trail = (int *)malloc(sizeof(int) * (formula->num_vars + 1));
    solver->trail_top = 0;

    solver->jw_scores = (double *)calloc(total_lit_slots, sizeof(double));
    for (int i = 0; i < formula->num_clauses; ++i)
    {
        Clause *c = &formula->clauses[i];
        double weight = std::pow(2.0, -c->size);
        for (int j = 0; j < c->size; ++j)
        {
            solver->jw_scores[lit_to_idx(c->lits[j])] += weight;
        }
    }

    for (int i = 0; i < formula->num_clauses; ++i)
    {
        Clause *c = &formula->clauses[i];
        if (c->size >= 2)
        {
            add_watch(&solver->watches[lit_to_idx(c->lits[0])], i, c->lits[1]);
            add_watch(&solver->watches[lit_to_idx(c->lits[1])], i, c->lits[0]);
        }
    }

    return solver;
}

void free_solver_2wl(Solver2WL *solver)
{
    if (!solver)
        return;
    int total_lit_slots = (solver->num_vars + 1) * 2;
    for (int i = 0; i < total_lit_slots; ++i)
    {
        if (solver->watches[i].list)
            free(solver->watches[i].list);
    }
    free(solver->watches);
    free(solver->assignment);
    free(solver->trail);
    free(solver->jw_scores);
    free(solver);
}

static bool bcp_2wl(Solver2WL *solver, int qhead)
{
    while (qhead < solver->trail_top)
    {
        int assigned_lit = solver->trail[qhead++];
        int false_lit = neg_lit(assigned_lit);
        int false_idx = lit_to_idx(false_lit);

        WatchList *ws = &solver->watches[false_idx];
        int j = 0;

        for (int i = 0; i < ws->size; ++i)
        {
            int clause_idx = ws->list[i].clause_idx;
            int blocker = ws->list[i].blocker;
            Clause *c = &solver->clauses[clause_idx];

            if (eval_lit(solver, blocker) == VAL_TRUE)
            {
                ws->list[j++] = ws->list[i];
                continue;
            }

            if (c->lits[0] == false_lit)
            {
                std::swap(c->lits[0], c->lits[1]);
            }

            int lit0 = c->lits[0];
            if (eval_lit(solver, lit0) == VAL_TRUE)
            {
                ws->list[i].blocker = lit0;
                ws->list[j++] = ws->list[i];
                continue;
            }

            bool found_new_watch = false;
            for (int k = 2; k < c->size; ++k)
            {
                if (eval_lit(solver, c->lits[k]) != VAL_FALSE)
                {
                    c->lits[1] = c->lits[k];
                    c->lits[k] = false_lit;
                    add_watch(&solver->watches[lit_to_idx(c->lits[1])], clause_idx, lit0);
                    found_new_watch = true;
                    break;
                }
            }

            if (found_new_watch)
                continue;

            ws->list[j++] = ws->list[i];
            int val0 = eval_lit(solver, lit0);
            if (val0 == VAL_FALSE)
            {
                while (++i < ws->size)
                    ws->list[j++] = ws->list[i];
                ws->size = j;
                return false;
            }
            else if (val0 == VAL_UNASSIGNED)
            {
                assign_literal(solver, lit0);
            }
        }
        ws->size = j;
    }
    return true;
}

static void backtrack_to_top(Solver2WL *solver, int target_top)
{
    while (solver->trail_top > target_top)
    {
        int lit = solver->trail[--solver->trail_top];
        solver->assignment[std::abs(lit)] = VAL_UNASSIGNED;
    }
}

static int pick_branch_lit_jw(const Solver2WL *solver)
{
    int best_var = 0;
    double max_score = -1.0;

    for (int v = 1; v <= solver->num_vars; ++v)
    {
        if (solver->assignment[v] == VAL_UNASSIGNED)
        {
            int pos_idx = lit_to_idx(v);
            int neg_idx = lit_to_idx(-v);
            double total_score = solver->jw_scores[pos_idx] + solver->jw_scores[neg_idx];

            if (total_score > max_score)
            {
                max_score = total_score;
                best_var = v;
            }
        }
    }

    if (best_var == 0)
        return 0;
    int pos_idx = lit_to_idx(best_var);
    int neg_idx = lit_to_idx(-best_var);
    return (solver->jw_scores[pos_idx] >= solver->jw_scores[neg_idx]) ? best_var : -best_var;
}

static SolverStatus dpll_2wl_dfs_timeout(Solver2WL *solver, Timer *timer, double timeout_ms, unsigned long long &call_count)
{
    if ((++call_count & TIMEOUT_CHECK_MASK) == 0)
    {
        if (timer->get_elapsed_ms() >= timeout_ms)
        {
            return STATUS_TIMEOUT;
        }
    }

    int branch_lit = pick_branch_lit_jw(solver);
    if (branch_lit == 0)
        return STATUS_SAT;

    int save_top = solver->trail_top;
    assign_literal(solver, branch_lit);

    if (bcp_2wl(solver, save_top))
    {
        SolverStatus res = dpll_2wl_dfs_timeout(solver, timer, timeout_ms, call_count);
        if (res != STATUS_UNSAT)
            return res;
    }
    backtrack_to_top(solver, save_top);

    assign_literal(solver, -branch_lit);
    if (bcp_2wl(solver, save_top))
    {
        SolverStatus res = dpll_2wl_dfs_timeout(solver, timer, timeout_ms, call_count);
        if (res != STATUS_UNSAT)
            return res;
    }
    backtrack_to_top(solver, save_top);

    return STATUS_UNSAT;
}

SolverStatus dpll_solve_optimized_timeout(Solver2WL *solver, double timeout_ms, double *elapsed_ms)
{
    Timer timer;
    timer.start();

    for (int i = 0; i < solver->num_clauses; ++i)
    {
        Clause *c = &solver->clauses[i];
        if (c->size == 1)
        {
            int val = eval_lit(solver, c->lits[0]);
            if (val == VAL_FALSE)
            {
                *elapsed_ms = timer.stop();
                return STATUS_UNSAT;
            }
            if (val == VAL_UNASSIGNED)
                assign_literal(solver, c->lits[0]);
        }
    }
    if (!bcp_2wl(solver, 0))
    {
        *elapsed_ms = timer.stop();
        return STATUS_UNSAT;
    }

    unsigned long long call_count = 0;
    SolverStatus status = dpll_2wl_dfs_timeout(solver, &timer, timeout_ms, call_count);
    *elapsed_ms = timer.stop();
    return status;
}