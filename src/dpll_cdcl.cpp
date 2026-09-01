#include "dpll_cdcl.h"
#include <cstdlib>
#include <cmath>
#include <algorithm>
#include <iostream>

// =================== 辅助映射函数 ===================
static inline int lit_to_idx(int lit) { return (lit > 0) ? (lit << 1) : ((-lit) << 1 | 1); }
static inline int neg_lit(int lit) { return -lit; }

static inline int eval_lit(const SolverCDCL *solver, int lit)
{
    int val = solver->assignment[std::abs(lit)];
    if (val == VAL_UNASSIGNED)
        return VAL_UNASSIGNED;
    return (lit > 0) ? val : (val ^ 1);
}

static void add_watch(CDCL_WatchList *wl, int clause_idx, int blocker)
{
    if (wl->size >= wl->capacity)
    {
        wl->capacity = (wl->capacity == 0) ? 4 : (wl->capacity * 2);
        wl->list = (CDCL_Watcher *)realloc(wl->list, sizeof(CDCL_Watcher) * wl->capacity);
    }
    wl->list[wl->size].clause_idx = clause_idx;
    wl->list[wl->size].blocker = blocker;
    wl->size++;
}

// 赋值、记录原因子句与决策层
static inline void unchecked_enqueue(SolverCDCL *solver, int lit, int reason_clause)
{
    int var = std::abs(lit);
    solver->assignment[var] = (lit > 0) ? VAL_TRUE : VAL_FALSE;
    solver->level[var] = solver->decision_level;
    solver->reason[var] = reason_clause;
    solver->saved_phase[var] = (lit > 0) ? 1 : 0; // 极性保存
    solver->trail[solver->trail_top++] = lit;

    // 关键不变量维护: 0 层赋值永远位于 trail 最前段, trail_lim[0] 必须紧跟其后。
    // 否则重启回跳 0 层时会把冲突分析得到的 0 层赋值一并抹掉 (trail_lim[0] 陈旧 bug)
    if (solver->decision_level == 0)
        solver->trail_lim[0] = solver->trail_top;
}

// =================== VSIDS 活跃度维护 ===================
static void var_bump_activity(SolverCDCL *solver, int var)
{
    solver->activity[var] += solver->var_inc;
    if (solver->activity[var] > 1e100)
    {
        // 防止浮点溢出，全部按比例缩小
        for (int i = 1; i <= solver->num_vars; ++i)
            solver->activity[i] *= 1e-100;
        solver->var_inc *= 1e-100;
    }
}

static void var_decay_activity(SolverCDCL *solver)
{
    solver->var_inc *= (1.0 / solver->var_decay);
}

// =================== Luby 重启序列生成 ===================
static int luby(int i)
{
    for (int k = 1; k < 32; ++k)
    {
        if (i == (1 << k) - 1)
            return 1 << (k - 1);
        if (i > (1 << (k - 1)) - 1 && i < (1 << k) - 1)
        {
            return luby(i - (1 << (k - 1)) + 1);
        }
    }
    return 1;
}

// =================== 2WL 单子句传播 (返回冲突子句指针) ===================
static Clause *bcp_cdcl(SolverCDCL *solver)
{
    while (solver->qhead < solver->trail_top)
    {
        int assigned_lit = solver->trail[solver->qhead++];
        int false_lit = neg_lit(assigned_lit);
        int false_idx = lit_to_idx(false_lit);

        CDCL_WatchList *ws = &solver->watches[false_idx];
        int j = 0;

        for (int i = 0; i < ws->size; ++i)
        {
            int clause_idx = ws->list[i].clause_idx;
            int blocker = ws->list[i].blocker;
            Clause *c = solver->clauses[clause_idx];

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

            bool found = false;
            for (int k = 2; k < c->size; ++k)
            {
                if (eval_lit(solver, c->lits[k]) != VAL_FALSE)
                {
                    c->lits[1] = c->lits[k];
                    c->lits[k] = false_lit;
                    add_watch(&solver->watches[lit_to_idx(c->lits[1])], clause_idx, lit0);
                    found = true;
                    break;
                }
            }

            if (found)
                continue;

            ws->list[j++] = ws->list[i];
            int val0 = eval_lit(solver, lit0);
            if (val0 == VAL_FALSE)
            {
                // 发生冲突，返回产生矛盾的子句
                while (++i < ws->size)
                    ws->list[j++] = ws->list[i];
                ws->size = j;
                solver->qhead = solver->trail_top; // 清空传播队列
                return c;
            }
            else if (val0 == VAL_UNASSIGNED)
            {
                unchecked_enqueue(solver, lit0, clause_idx);
                solver->stats.propagations++; // 统计传播赋值次数
            }
        }
        ws->size = j;
    }
    return nullptr;
}

// =================== 非时序回跳 (Backjump) ===================
static void backjump_to_level(SolverCDCL *solver, int target_level)
{
    if (solver->decision_level <= target_level)
        return;

    int target_top = solver->trail_lim[target_level];
    while (solver->trail_top > target_top)
    {
        int lit = solver->trail[--solver->trail_top];
        int var = std::abs(lit);
        solver->assignment[var] = VAL_UNASSIGNED;
        solver->reason[var] = -1;
    }
    solver->qhead = target_top;
    solver->decision_level = target_level;
}

// =================== 学习子句库定期缩减 (LBD 打分 + 锁定保护) ===================
static SolverCDCL *g_reduce_solver; // qsort 比较器无法传参, 借用文件级指针

static int cmp_lbd_desc(const void *a, const void *b)
{
    int ia = *(const int *)a, ib = *(const int *)b;
    int la = g_reduce_solver->clauses[ia]->lbd;
    int lb = g_reduce_solver->clauses[ib]->lbd;
    return (la < lb) - (la > lb); // LBD 大者在前 (优先删除)
}

static void reduce_clause_db(SolverCDCL *solver)
{
    int n_learnt = solver->clauses_size - solver->num_clauses;
    if (n_learnt <= 0)
        return;

    // 1. 收集现存学习子句下标, 全部标为"删除候选"
    int n = 0;
    int *idx = (int *)malloc(sizeof(int) * n_learnt);
    for (int i = solver->num_clauses; i < solver->clauses_size; ++i)
    {
        if (solver->clauses[i])
        {
            idx[n++] = i;
            solver->del_mark[i] = true;
        }
    }

    // 2. 锁定保护: trail 上仍作为 reason 的学习子句不可删除
    for (int t = 0; t < solver->trail_top; ++t)
    {
        int r = solver->reason[std::abs(solver->trail[t])];
        if (r >= solver->num_clauses)
            solver->del_mark[r] = false;
    }

    // 3. 按 LBD 降序排序, 保留前半 + LBD<=2 / 长度<=2 的高价值子句
    g_reduce_solver = solver;
    qsort(idx, n, sizeof(int), cmp_lbd_desc);
    int keep = n / 2;
    for (int k = 0; k < n; ++k)
    {
        int i = idx[k];
        Clause *c = solver->clauses[i];
        if (k < keep || c->lbd <= 2 || c->size <= 2)
            solver->del_mark[i] = false;
    }

    // 4. 重建所有观察表, 剔除被删子句的 watcher
    int total_slots = (solver->num_vars + 1) * 2;
    for (int s = 0; s < total_slots; ++s)
    {
        CDCL_WatchList *wl = &solver->watches[s];
        int j = 0;
        for (int i = 0; i < wl->size; ++i)
        {
            int cidx = wl->list[i].clause_idx;
            if (cidx >= solver->num_clauses && solver->del_mark[cidx])
                continue;
            wl->list[j++] = wl->list[i];
        }
        wl->size = j;
    }

    // 5. 释放被删子句内存
    for (int k = 0; k < n; ++k)
    {
        int i = idx[k];
        if (solver->del_mark[i])
        {
            delete[] solver->clauses[i]->lits;
            delete solver->clauses[i];
            solver->clauses[i] = nullptr;
        }
    }
    free(idx);
}

// =================== 核心：1-UIP 冲突分析与子句学习 ===================
static void analyze_conflict(SolverCDCL *solver, Clause *conf_clause, int *out_lits, int *out_size, int *out_bt_level, int *out_lbd)
{
    int path_cnt = 0;
    int p = 0;
    int cur_level = solver->decision_level;
    int idx = solver->trail_top - 1;

    *out_size = 1; // 下标 0 留给 1-UIP 文字
    *out_bt_level = 0;

    Clause *c = conf_clause;

    while (true)
    {
        // 对当前子句中的文字进行布尔消解
        for (int j = (p == 0 ? 0 : 1); j < c->size; ++j)
        {
            int lit = c->lits[j];
            int var = std::abs(lit);

            if (!solver->seen[var] && solver->level[var] > 0)
            {
                solver->seen[var] = true;
                var_bump_activity(solver, var); // 增加冲突变元活跃度

                if (solver->level[var] >= cur_level)
                {
                    path_cnt++; // 属于当前决策层的文字
                }
                else
                {
                    out_lits[(*out_size)++] = lit; // 属于更早决策层的文字，直接加入学习子句
                    if (solver->level[var] > *out_bt_level)
                    {
                        *out_bt_level = solver->level[var]; // 寻找第二大决策层
                    }
                }
            }
        }

        // 沿着 Trail 栈逆向寻找下一个需要消解的变元
        while (!solver->seen[std::abs(solver->trail[idx])])
            idx--;

        p = solver->trail[idx--];
        int var_p = std::abs(p);
        solver->seen[var_p] = false;
        path_cnt--;

        if (path_cnt <= 0)
        {
            // 找到了 1-UIP (第一唯一蕴涵点)！
            out_lits[0] = neg_lit(p); // 1-UIP 文字取反后作为 Asserting Literal 放在首位
            break;
        }

        c = solver->clauses[solver->reason[var_p]];
    }

    // 清理 seen 标记
    for (int i = 0; i < *out_size; ++i)
    {
        solver->seen[std::abs(out_lits[i])] = false;
    }

    // 计算 LBD (Glue): 学习子句涉及的决策层数。
    // 1-UIP 文字位于当前决策层 (计 1 层), 其余文字统计不同决策层个数。
    int lbd = 1;
    for (int i = 1; i < *out_size; ++i)
    {
        int lv = solver->level[std::abs(out_lits[i])];
        if (!solver->level_mark[lv])
        {
            solver->level_mark[lv] = true;
            lbd++;
        }
    }
    for (int i = 1; i < *out_size; ++i)
    {
        solver->level_mark[solver->level[std::abs(out_lits[i])]] = false;
    }
    *out_lbd = lbd;
}

// =================== VSIDS 变元分支决策 ===================
static int pick_branch_lit_vsids(const SolverCDCL *solver)
{
    int best_var = 0;
    double max_act = -1.0;

    for (int v = 1; v <= solver->num_vars; ++v)
    {
        if (solver->assignment[v] == VAL_UNASSIGNED)
        {
            if (solver->activity[v] > max_act)
            {
                max_act = solver->activity[v];
                best_var = v;
            }
        }
    }

    if (best_var == 0)
        return 0;
    // 结合极性记忆
    return (solver->saved_phase[best_var] == 1) ? best_var : -best_var;
}

// =================== 初始化与销毁 ===================
SolverCDCL *create_solver_cdcl(const CNFFormula *formula)
{
    SolverCDCL *solver = (SolverCDCL *)malloc(sizeof(SolverCDCL));
    solver->num_vars = formula->num_vars;
    solver->num_clauses = formula->num_clauses;

    // 子句库预留 4 倍空间用于存放学习子句
    solver->clauses_cap = formula->num_clauses * 4 + 10000;
    solver->clauses_size = formula->num_clauses;
    solver->clauses = (Clause **)malloc(sizeof(Clause *) * solver->clauses_cap);
    solver->del_mark = (bool *)malloc(sizeof(bool) * solver->clauses_cap);
    for (int i = 0; i < formula->num_clauses; ++i)
    {
        solver->clauses[i] = &formula->clauses[i];
        solver->clauses[i]->learnt = false;
        solver->clauses[i]->lbd = 0;
    }

    int total_lit_slots = (formula->num_vars + 1) * 2;
    solver->watches = (CDCL_WatchList *)calloc(total_lit_slots, sizeof(CDCL_WatchList));
    solver->assignment = (int *)malloc(sizeof(int) * (formula->num_vars + 1));
    solver->level = (int *)malloc(sizeof(int) * (formula->num_vars + 1));
    solver->reason = (int *)malloc(sizeof(int) * (formula->num_vars + 1));
    solver->saved_phase = (int *)malloc(sizeof(int) * (formula->num_vars + 1));
    solver->seen = (bool *)calloc(formula->num_vars + 1, sizeof(bool));
    solver->level_mark = (bool *)calloc(formula->num_vars + 1, sizeof(bool));
    solver->activity = (double *)calloc(formula->num_vars + 1, sizeof(double));

    for (int i = 0; i <= formula->num_vars; ++i)
    {
        solver->assignment[i] = VAL_UNASSIGNED;
        solver->level[i] = -1;
        solver->reason[i] = -1;
        solver->saved_phase[i] = 1; // 默认猜 True
    }

    solver->trail = (int *)malloc(sizeof(int) * (formula->num_vars + 1));
    solver->trail_lim = (int *)malloc(sizeof(int) * (formula->num_vars + 1));
    solver->trail_top = 0;
    solver->qhead = 0;
    solver->decision_level = 0;
    solver->trail_lim[0] = 0; // 见 unchecked_enqueue 中的不变量维护

    solver->var_inc = 1.0;
    solver->var_decay = 0.95;

    solver->conflicts = 0;
    solver->restart_inc = 100;
    solver->luby_u = 1;
    solver->next_restart = solver->restart_inc; // 从当前冲突数累加, 避免重启连发
    solver->next_reduce = 2000;                 // 每 2000 次冲突缩减一次学习子句库

    solver->stats.decisions = 0;
    solver->stats.propagations = 0;
    solver->stats.conflicts = 0;
    solver->stats.restarts = 0;

    // 建立 2WL
    for (int i = 0; i < formula->num_clauses; ++i)
    {
        Clause *c = solver->clauses[i];
        if (c->size >= 2)
        {
            add_watch(&solver->watches[lit_to_idx(c->lits[0])], i, c->lits[1]);
            add_watch(&solver->watches[lit_to_idx(c->lits[1])], i, c->lits[0]);
        }
    }

    return solver;
}

void free_solver_cdcl(SolverCDCL *solver)
{
    if (!solver)
        return;
    int total_lit_slots = (solver->num_vars + 1) * 2;
    for (int i = 0; i < total_lit_slots; ++i)
    {
        if (solver->watches[i].list)
            free(solver->watches[i].list);
    }
    // 释放动态生成的学习子句
    for (int i = solver->num_clauses; i < solver->clauses_size; ++i)
    {
        if (solver->clauses[i])
        {
            delete[] solver->clauses[i]->lits;
            delete solver->clauses[i];
        }
    }
    free(solver->clauses);
    free(solver->del_mark);
    free(solver->watches);
    free(solver->assignment);
    free(solver->level);
    free(solver->reason);
    free(solver->saved_phase);
    free(solver->seen);
    free(solver->level_mark);
    free(solver->activity);
    free(solver->trail);
    free(solver->trail_lim);
    free(solver);
}

// =================== CDCL 迭代求解主引擎 ===================
SolverStatus cdcl_solve_timeout(SolverCDCL *solver, double timeout_ms, double *elapsed_ms)
{
    Timer timer;
    timer.start();

    // 0 层预处理
    for (int i = 0; i < solver->num_clauses; ++i)
    {
        Clause *c = solver->clauses[i];
        if (c->size == 1)
        {
            int val = eval_lit(solver, c->lits[0]);
            if (val == VAL_FALSE)
            {
                *elapsed_ms = timer.stop();
                return STATUS_UNSAT;
            }
            if (val == VAL_UNASSIGNED)
                unchecked_enqueue(solver, c->lits[0], i);
        }
    }

    if (bcp_cdcl(solver) != nullptr)
    {
        *elapsed_ms = timer.stop();
        return STATUS_UNSAT;
    }

    int *learned_buf = (int *)malloc(sizeof(int) * (solver->num_vars + 1));
    unsigned long long step_count = 0;

    while (true)
    {
        // 超时检测
        if ((++step_count & TIMEOUT_CHECK_MASK) == 0)
        {
            if (timer.get_elapsed_ms() >= timeout_ms)
            {
                free(learned_buf);
                *elapsed_ms = timer.stop();
                return STATUS_TIMEOUT;
            }
        }

        // 1. 单子句传播
        Clause *conf_clause = bcp_cdcl(solver);

        if (conf_clause != nullptr)
        {
            // 发生冲突！
            solver->conflicts++;
            solver->stats.conflicts++;
            var_decay_activity(solver); // 衰减全局活跃度

            if (solver->decision_level == 0)
            {
                free(learned_buf);
                *elapsed_ms = timer.stop();
                return STATUS_UNSAT; // 0 层冲突，公式恒不可满足！
            }

            // 2. 1-UIP 冲突分析
            int learned_size = 0;
            int backtrack_level = 0;
            int learned_lbd = 0;
            analyze_conflict(solver, conf_clause, learned_buf, &learned_size, &backtrack_level, &learned_lbd);

            // 3. 非时序跨层回跳 (Backjump)
            backjump_to_level(solver, backtrack_level);

            // 4. 将学习子句注入数据库并建立 2WL
            Clause *learned_c = new Clause();
            learned_c->size = learned_size;
            learned_c->original_size = learned_size;
            learned_c->is_satisfied = false;
            learned_c->learnt = true;
            learned_c->lbd = learned_lbd;
            learned_c->lits = new int[learned_size];
            for (int i = 0; i < learned_size; ++i)
                learned_c->lits[i] = learned_buf[i];

            int new_c_idx = solver->clauses_size++;
            if (solver->clauses_size >= solver->clauses_cap)
            {
                solver->clauses_cap *= 2;
                solver->clauses = (Clause **)realloc(solver->clauses, sizeof(Clause *) * solver->clauses_cap);
                solver->del_mark = (bool *)realloc(solver->del_mark, sizeof(bool) * solver->clauses_cap);
            }
            solver->clauses[new_c_idx] = learned_c;

            if (learned_size >= 2)
            {
                add_watch(&solver->watches[lit_to_idx(learned_c->lits[0])], new_c_idx, learned_c->lits[1]);
                add_watch(&solver->watches[lit_to_idx(learned_c->lits[1])], new_c_idx, learned_c->lits[0]);
            }

            // 5. 学习子句驱动传播：1-UIP 成为 Asserting Literal，强制赋值！
            unchecked_enqueue(solver, learned_c->lits[0], new_c_idx);
            solver->stats.propagations++;

            // 6. 学习子句库定期缩减：按 LBD 删除低价值子句, 防止数据库无限膨胀拖慢 BCP
            if (solver->conflicts >= solver->next_reduce)
            {
                reduce_clause_db(solver);
                solver->next_reduce = solver->conflicts + 2000;
            }
        }
        else
        {
            // 无冲突，检查 Luby 动态重启（阈值从当前冲突数累加, 避免重启连发）
            if (solver->conflicts >= solver->next_restart)
            {
                solver->next_restart = solver->conflicts + solver->restart_inc * luby(++solver->luby_u);
                solver->stats.restarts++;
                backjump_to_level(solver, 0); // 回跳到第 0 层（保留活跃度和极性）
            }

            // 7. 分支决策
            int branch_lit = pick_branch_lit_vsids(solver);
            if (branch_lit == 0)
            {
                // 所有变元已合法赋值，求解成功！
                free(learned_buf);
                *elapsed_ms = timer.stop();
                return STATUS_SAT;
            }

            // 推进决策层
            solver->trail_lim[solver->decision_level++] = solver->trail_top;
            unchecked_enqueue(solver, branch_lit, -1);
            solver->stats.decisions++;
        }
    }
}