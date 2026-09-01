#include "dpll_baseline.h"
#include <cmath>
#include <iostream>

static inline int get_literal_value(int lit, const int *assignment)
{
    int var = std::abs(lit);
    int val = assignment[var];
    if (val == VAL_UNASSIGNED)
        return VAL_UNASSIGNED;
    if (lit > 0)
        return val;
    return (val == VAL_TRUE) ? VAL_FALSE : VAL_TRUE;
}

static int check_clause_status(const Clause *clause, const int *assignment, int *unit_lit)
{
    int unassigned_count = 0;
    int last_unassigned_lit = 0;

    for (int i = 0; i < clause->size; ++i)
    {
        int lit = clause->lits[i];
        int val = get_literal_value(lit, assignment);
        if (val == VAL_TRUE)
            return 1;
        if (val == VAL_UNASSIGNED)
        {
            unassigned_count++;
            last_unassigned_lit = lit;
        }
    }

    if (unassigned_count == 0)
        return -1;
    if (unassigned_count == 1 && unit_lit)
        *unit_lit = last_unassigned_lit;
    return 0;
}

static bool unit_propagate_baseline(const CNFFormula *formula, int *assignment, int *forced_vars, int *forced_count, SolverStats *stats)
{
    bool has_unit = true;
    *forced_count = 0;

    while (has_unit)
    {
        has_unit = false;
        for (int i = 0; i < formula->num_clauses; ++i)
        {
            int unit_lit = 0;
            int status = check_clause_status(&formula->clauses[i], assignment, &unit_lit);

            if (status == -1)
            {
                if (stats)
                    stats->conflicts++;
                return false;
            }
            if (status == 0 && unit_lit != 0)
            {
                int var = std::abs(unit_lit);
                assignment[var] = (unit_lit > 0) ? VAL_TRUE : VAL_FALSE;
                forced_vars[(*forced_count)++] = var;
                if (stats)
                    stats->propagations++;
                has_unit = true;
                break;
            }
        }
    }
    return true;
}

static bool is_formula_satisfied(const CNFFormula *formula, const int *assignment)
{
    for (int i = 0; i < formula->num_clauses; ++i)
    {
        if (check_clause_status(&formula->clauses[i], assignment, nullptr) != 1)
        {
            return false;
        }
    }
    return true;
}

static int pick_branching_variable_baseline(int num_vars, const int *assignment)
{
    for (int v = 1; v <= num_vars; ++v)
    {
        if (assignment[v] == VAL_UNASSIGNED)
            return v;
    }
    return 0;
}

// 递归主体
static SolverStatus dpll_baseline_core(const CNFFormula *formula, int *assignment,
                                       Timer *timer, double timeout_ms, unsigned long long &call_count,
                                       SolverStats *stats)
{
    // 周期性超时检测
    if ((++call_count & TIMEOUT_CHECK_MASK) == 0)
    {
        if (timer->get_elapsed_ms() >= timeout_ms)
        {
            return STATUS_TIMEOUT;
        }
    }

    int *forced_vars = new int[formula->num_vars + 1];
    int forced_count = 0;

    if (!unit_propagate_baseline(formula, assignment, forced_vars, &forced_count, stats))
    {
        for (int i = 0; i < forced_count; ++i)
            assignment[forced_vars[i]] = VAL_UNASSIGNED;
        delete[] forced_vars;
        return STATUS_UNSAT;
    }

    if (is_formula_satisfied(formula, assignment))
    {
        delete[] forced_vars;
        return STATUS_SAT;
    }

    int branch_var = pick_branching_variable_baseline(formula->num_vars, assignment);
    if (branch_var == 0)
    {
        for (int i = 0; i < forced_count; ++i)
            assignment[forced_vars[i]] = VAL_UNASSIGNED;
        delete[] forced_vars;
        return STATUS_UNSAT;
    }

    // 尝试 True 分支
    assignment[branch_var] = VAL_TRUE;
    if (stats)
        stats->decisions++;
    SolverStatus res = dpll_baseline_core(formula, assignment, timer, timeout_ms, call_count, stats);
    if (res == STATUS_SAT || res == STATUS_TIMEOUT)
    {
        delete[] forced_vars;
        return res;
    }

    // 尝试 False 分支
    assignment[branch_var] = VAL_FALSE;
    if (stats)
        stats->decisions++;
    res = dpll_baseline_core(formula, assignment, timer, timeout_ms, call_count, stats);
    if (res == STATUS_SAT || res == STATUS_TIMEOUT)
    {
        delete[] forced_vars;
        return res;
    }

    // 回溯
    assignment[branch_var] = VAL_UNASSIGNED;
    for (int i = 0; i < forced_count; ++i)
        assignment[forced_vars[i]] = VAL_UNASSIGNED;
    delete[] forced_vars;
    return STATUS_UNSAT;
}

SolverStatus dpll_solve_baseline_timeout(const CNFFormula *formula, int *assignment, double timeout_ms, double *elapsed_ms, SolverStats *stats)
{
    Timer timer;
    timer.start();
    unsigned long long call_count = 0;
    SolverStatus status = dpll_baseline_core(formula, assignment, &timer, timeout_ms, call_count, stats);
    *elapsed_ms = timer.stop();
    return status;
}