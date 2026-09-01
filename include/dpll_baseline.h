#include <iostream>
#include <cstring>
#include <cmath>
#include "types.h"

// 辅助函数：根据当前赋值获取文字的值
// lit > 0 表示正文字，lit < 0 表示负文字
inline int get_literal_value(int lit, const int *assignment)
{
    int var = std::abs(lit);
    int val = assignment[var];
    if (val == VAL_UNASSIGNED)
        return VAL_UNASSIGNED;
    if (lit > 0)
        return val;                                  // 正文字：变元为1则为1，变元为0则为0
    return (val == VAL_TRUE) ? VAL_FALSE : VAL_TRUE; // 负文字：取反
}

// 辅助函数：检查子句状态
// 返回值：1 表示已满足，0 表示未满足，-1 表示冲突（全部为假）
// 如果是单子句，通过 unit_lit 传出那个唯一的未赋值文字
int check_clause_status(const Clause *clause, const int *assignment, int *unit_lit)
{
    int unassigned_count = 0;
    int last_unassigned_lit = 0;

    for (int i = 0; i < clause->size; ++i)
    {
        int lit = clause->lits[i];
        int val = get_literal_value(lit, assignment);

        if (val == VAL_TRUE)
        {
            return 1; // 子句已被满足
        }
        if (val == VAL_UNASSIGNED)
        {
            unassigned_count++;
            last_unassigned_lit = lit;
        }
    }

    if (unassigned_count == 0)
    {
        return -1; // 发生冲突：所有文字均为假
    }
    if (unassigned_count == 1)
    {
        if (unit_lit)
            *unit_lit = last_unassigned_lit;
    }
    return 0; // 仍未满足，且有多个未赋值文字
}

// 1. 单子句传播 (Boolean Constraint Propagation, BCP)
// 返回 false 表示检测到冲突 (UNSAT)，返回 true 表示传播顺利完成
bool unit_propagate_baseline(const CNFFormula *formula, int *assignment, int *forced_vars, int *forced_count)
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
                return false; // 冲突！
            }
            if (status == 0 && unit_lit != 0)
            {
                // 找到单子句，必须强制赋值使其为真
                int var = std::abs(unit_lit);
                assignment[var] = (unit_lit > 0) ? VAL_TRUE : VAL_FALSE;

                // 记录本轮 BCP 强制赋值的变量，用于冲突时回溯
                forced_vars[(*forced_count)++] = var;
                has_unit = true; // 产生新赋值，继续循环检查是否引发连锁传播
                break;           // 重新扫描所有子句
            }
        }
    }
    return true;
}

// 2. 检查公式是否已经完全满足
bool is_formula_satisfied(const CNFFormula *formula, const int *assignment)
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

// 3. 朴素变元选取策略：顺序找第一个未赋值的变元
int pick_branching_variable_baseline(int num_vars, const int *assignment)
{
    for (int v = 1; v <= num_vars; ++v)
    {
        if (assignment[v] == VAL_UNASSIGNED)
        {
            return v;
        }
    }
    return 0; // 0 表示所有变元均已赋值
}

// 4. 递归 DPLL 核心过程
bool dpll_recursive_baseline(const CNFFormula *formula, int *assignment)
{
    // 准备记录单子句传播中强制赋值的变量（栈上分配）
    int *forced_vars = new int[formula->num_vars + 1];
    int forced_count = 0;

    // 步骤 A: 执行单子句传播
    if (!unit_propagate_baseline(formula, assignment, forced_vars, &forced_count))
    {
        // 发生冲突，撤销本层 BCP 的赋值
        for (int i = 0; i < forced_count; ++i)
        {
            assignment[forced_vars[i]] = VAL_UNASSIGNED;
        }
        delete[] forced_vars;
        return false;
    }

    // 步骤 B: 检查是否已全部满足
    if (is_formula_satisfied(formula, assignment))
    {
        delete[] forced_vars;
        return true;
    }

    // 步骤 C: 选取分支变元
    int branch_var = pick_branching_variable_baseline(formula->num_vars, assignment);
    if (branch_var == 0)
    {
        // 无可用变元且未完全满足（可能存在无解子句）
        for (int i = 0; i < forced_count; ++i)
        {
            assignment[forced_vars[i]] = VAL_UNASSIGNED;
        }
        delete[] forced_vars;
        return false;
    }

    // 步骤 D: 尝试分支 (先尝试赋 True)
    assignment[branch_var] = VAL_TRUE;
    if (dpll_recursive_baseline(formula, assignment))
    {
        delete[] forced_vars;
        return true;
    }

    // 步骤 E: 回溯，尝试反向分支 (赋 False)
    assignment[branch_var] = VAL_FALSE;
    if (dpll_recursive_baseline(formula, assignment))
    {
        delete[] forced_vars;
        return true;
    }

    // 步骤 F: 两个分支均失败，恢复本层所有改动并回退
    assignment[branch_var] = VAL_UNASSIGNED;
    for (int i = 0; i < forced_count; ++i)
    {
        assignment[forced_vars[i]] = VAL_UNASSIGNED;
    }
    delete[] forced_vars;
    return false;
}

