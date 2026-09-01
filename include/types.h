// include/types.h
#ifndef TYPES_H
#define TYPES_H

#include <stdbool.h>

// 变元真值状态
typedef enum
{
    VAL_UNASSIGNED = -1,
    VAL_FALSE = 0,
    VAL_TRUE = 1
} VarValue;

// 求解器返回状态
typedef enum
{
    SAT_UNKNOWN = 0,
    SAT_SATISFIABLE = 1,
    SAT_UNSATISFIABLE = 2,
    SAT_TIMEOUT = 3,
    SAT_PARSE_ERROR = 4
} SolverResult;

/* ================= 物理子句与公式结构 ================= */

// 单个子句结构
typedef struct
{
    int *lits;         // 文字数组指针（正数表示正文字，负数表示负文字）
    int size;          // 子句当前有效文字数（DPLL消除过程中变动）
    int original_size; // 子句原始文字数（用于回溯和重置）
    bool is_satisfied; // 当前是否已被满足
} Clause;

// 整个合取范式 (CNF) 物理存储
typedef struct
{
    int num_vars;    // 变元总数 N (编号 1 ~ N)
    int num_clauses; // 子句总数 M
    Clause *clauses; // 子句数组 (连续内存空间，长度 M)

    // 连续文字内存池（避免每个子句独立 malloc 一次）
    int *lit_pool;         // 扁平化存储所有文字的总缓冲区
    int lit_pool_capacity; // 文字池最大容量
    int lit_pool_used;     // 已使用的文字数量
} CNFFormula;

/* ================= 决策树与回溯状态栈 ================= */

// 回溯栈中的一次操作记录（用于无递归快速回溯）
typedef struct
{
    int var;            // 赋值的变元编号
    VarValue val;       // 赋的值 (VAL_TRUE / VAL_FALSE)
    int decision_level; // 决策层级 (0为单子句传播推导，>0为分支猜测)
    bool is_forced;     // 是否为单子句推导强制赋值 (Unit Propagation)
} TrailEntry;

typedef struct
{
    TrailEntry *stack;  // 显式分配的栈空间，大小为 num_vars + 1
    int top;            // 栈顶指针
    int *level_offsets; // 记录每个 decision_level 在栈中的起始位置
} Trail;

#endif