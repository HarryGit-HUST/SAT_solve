#pragma once

#include "types.h"
#include "config.h"
#include "timer.h"

// 观察项
struct CDCL_Watcher {
    int clause_idx;
    int blocker;
};

struct CDCL_WatchList {
    CDCL_Watcher* list;
    int size;
    int capacity;
};

struct SolverCDCL
{
    int num_vars;    // 变量总数
    int num_clauses; // 原始CNF子句数量（不含学习得到的子句）

    // 子句动态数据库，存所有子句：原始子句 + 学习生成的子句
    Clause **clauses; // 子句指针数组 Clause* clauses[]
    int clauses_size; // 当前有效子句个数
    int clauses_cap;  // 数组容量，不够就realloc扩容

    CDCL_WatchList *watches; // 【2‑watch核心】监视列表数组，每个文字对应一个watch列表

    // ==========赋值、蕴涵图信息==========
    int *assignment;  // 变量赋值：VAL_UNASSIGNED(-1), VAL_FALSE(0), VAL_TRUE(1)，下标是变量编号
    int *level;       // level[x]：变量x是在哪一个决策层级被赋值
    int *reason;      // reason[x]：什么子句推导出x赋值；-1=这是用户手动选的**决策变量**，不是传播出来
    int *saved_phase; // phase saving 相位保存：回退时记住变量上次选的极性，减少分支失败

    // ==========传播轨迹 trail 时间线栈（CDCL最重要的数据结构）==========
    int *trail;         // 赋值轨迹栈：存已经被赋值的**文字(lit)**，按赋值顺序压栈
    int *trail_lim;     // trail_lim[d]：第d层决策，在trail数组中的起始位置下标，用于回退(backtrack)
    int trail_top;      // trail栈顶，等价于已经赋值的文字总数量
    int qhead;          // BCP传播队列指针！！bcp_cdcl核心变量，trail同时充当传播队列，qhead是待处理位置
    int decision_level; // 当前决策层级，初始0，每做一次决策+1

    // ==========VSIDS 分支启发（选哪个变量来分裂）==========
    double *activity; // 每个变量活跃度，冲突越多分数越高，优先选高分变量
    double var_inc;   // 冲突后给变量增加多少活跃度
    double var_decay; // 活跃度衰减系数，一般0.95，旧冲突慢慢降权

    bool *seen; // 冲突分析1‑UIP算法的临时标记数组，标记变量是否访问过

    // ==========重启Luby序列==========
    int conflicts; // 已经发生冲突总次数
    int restart_inc;
    int luby_u;
    int luby_v;
    long long next_restart; // 到达该冲突数就执行重启

    // ==========学习子句删减管理==========
    bool *del_mark;        // 删减子句时标记哪些学习子句要删掉
    bool *level_mark;      // 计算LBD值的时候标记出现过的决策层
    long long next_reduce; // 冲突数到达这个值，执行一次学习子句库缩减

    SolverStats stats; // 统计：传播次数、冲突数、决策数、求解时间，输出res结果文件
};

// 接口
SolverCDCL* create_solver_cdcl(const CNFFormula* formula);
void free_solver_cdcl(SolverCDCL* solver);
SolverStatus cdcl_solve_timeout(SolverCDCL* solver, double timeout_ms, double* elapsed_ms);