#include "dpll_optimized.h"
#include <cstdlib>
#include <cmath>
#include <algorithm>
#include <iostream>

// =================== 文字与索引映射辅助函数 ===================
static inline int lit_to_idx(int lit)
{
    return (lit > 0) ? (lit << 1) : ((-lit) << 1 | 1);
}

static inline int idx_to_lit(int idx)
{
    return (idx & 1) ? -(idx >> 1) : (idx >> 1);
}

static inline int neg_lit(int lit)
{
    return -lit;
}

// 动态添加观察项
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

// 获取文字在当前 assignment 下的值
static inline int eval_lit(const Solver2WL *solver, int lit)
{
    int var = std::abs(lit);
    int val = solver->assignment[var];
    if (val == VAL_UNASSIGNED)
        return VAL_UNASSIGNED;
    return (lit > 0) ? val : (val ^ 1);
}

// 赋值并推入 Trail 栈
static inline void assign_literal(Solver2WL *solver, int lit)
{
    int var = std::abs(lit);
    solver->assignment[var] = (lit > 0) ? VAL_TRUE : VAL_FALSE;
    solver->trail[solver->trail_top++] = lit;
}

// =================== 初始化求解器与 2WL 构建 ===================
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

    // 计算 Jeroslow-Wang 启发式初始权重
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

    // 初始化双观察指针 (为每个长度 >= 2 的子句观察前两个文字)
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

// =================== 核心：基于 2WL 的极速 BCP ===================
static bool bcp_2wl(Solver2WL *solver, int qhead)
{
    while (qhead < solver->trail_top)
    {
        int assigned_lit = solver->trail[qhead++];
        int false_lit = neg_lit(assigned_lit); // 该文字被证伪
        int false_idx = lit_to_idx(false_lit);

        WatchList *ws = &solver->watches[false_idx];
        int j = 0; // 紧缩指针（原地重构观察表）

        for (int i = 0; i < ws->size; ++i)
        {
            int clause_idx = ws->list[i].clause_idx;
            int blocker = ws->list[i].blocker;
            Clause *c = &solver->clauses[clause_idx];

            // 1. Blocker 快速剪枝：如果 blocker 已经为 True，子句必定已满足，直接跳过！
            if (eval_lit(solver, blocker) == VAL_TRUE)
            {
                ws->list[j++] = ws->list[i];
                continue;
            }

            // 确保 c->lits[1] 是当前变为 False 的这个文字 false_lit
            if (c->lits[0] == false_lit)
            {
                std::swap(c->lits[0], c->lits[1]);
            }

            // 检查另一个观察文字 c->lits[0]
            int lit0 = c->lits[0];
            if (eval_lit(solver, lit0) == VAL_TRUE)
            {
                ws->list[i].blocker = lit0; // 更新 blocker 缓存
                ws->list[j++] = ws->list[i];
                continue;
            }

            // 2. 在子句中寻找新的非 False 文字来替换 false_lit
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
                continue; // 成功迁移，不保留在当前 false_lit 的观察表

            // 3. 没找到替换文字：判定单子句或冲突
            ws->list[j++] = ws->list[i]; // 保留该观察项

            int val0 = eval_lit(solver, lit0);
            if (val0 == VAL_FALSE)
            {
                // 冲突：另一个观察文字也是 False，发生矛盾！
                while (++i < ws->size)
                    ws->list[j++] = ws->list[i];
                ws->size = j;
                return false; // 触发冲突
            }
            else if (val0 == VAL_UNASSIGNED)
            {
                // 单子句传播：强制对 lit0 赋值为 True
                assign_literal(solver, lit0);
            }
        }
        ws->size = j;
    }
    return true;
}

// =================== 秒级 Trail 栈回溯 ===================
static void backtrack_to_top(Solver2WL *solver, int target_top)
{
    while (solver->trail_top > target_top)
    {
        int lit = solver->trail[--solver->trail_top];
        solver->assignment[std::abs(lit)] = VAL_UNASSIGNED; // 仅修改变元状态，观察表零修改！
    }
}

// =================== JW 分支启发式策略 ===================
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
        return 0; // 全部赋值完毕

    // 极性选择：优先选得分更高的分支方向
    int pos_idx = lit_to_idx(best_var);
    int neg_idx = lit_to_idx(-best_var);
    return (solver->jw_scores[pos_idx] >= solver->jw_scores[neg_idx]) ? best_var : -best_var;
}

// =================== 2WL 递归回溯主引擎 ===================
static bool dpll_2wl_dfs(Solver2WL *solver)
{
    // 1. 启发式选取分支文字
    int branch_lit = pick_branch_lit_jw(solver);
    if (branch_lit == 0)
        return true; // 所有变元已合法赋值 -> SAT!

    // 2. 尝试正向分支
    int save_top = solver->trail_top;
    assign_literal(solver, branch_lit);

    if (bcp_2wl(solver, save_top))
    {
        if (dpll_2wl_dfs(solver))
            return true;
    }
    // 正向失败，回溯
    backtrack_to_top(solver, save_top);

    // 3. 尝试反向分支
    assign_literal(solver, -branch_lit);

    if (bcp_2wl(solver, save_top))
    {
        if (dpll_2wl_dfs(solver))
            return true;
    }
    // 反向也失败，回溯并返回 false
    backtrack_to_top(solver, save_top);

    return false;
}

// =================== 对外求解接口 ===================
bool dpll_solve_optimized(Solver2WL *solver)
{
    // 0 层预处理：处理初始长度为 1 的单子句
    for (int i = 0; i < solver->num_clauses; ++i)
    {
        Clause *c = &solver->clauses[i];
        if (c->size == 1)
        {
            int val = eval_lit(solver, c->lits[0]);
            if (val == VAL_FALSE)
                return false;
            if (val == VAL_UNASSIGNED)
                assign_literal(solver, c->lits[0]);
        }
    }

    // 0 层初始单子句传播
    if (!bcp_2wl(solver, 0))
        return false;

    // 进入核心搜索
    return dpll_2wl_dfs(solver);
}