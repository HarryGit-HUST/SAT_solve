#pragma once

#include <stdbool.h>

// 变元真值状态
typedef enum
{
    VAL_UNASSIGNED = -1,
    VAL_FALSE = 0,
    VAL_TRUE = 1
} VarValue;

// 求解器返回状态码（完全对应任务书 s 1, s 0, s -1）
typedef enum
{
    STATUS_TIMEOUT = -1, // s -1: 超时未完成
    STATUS_UNSAT = 0,    // s  0: 证明不可满足
    STATUS_SAT = 1       // s  1: 找到可满足解
} SolverStatus;

// 单个子句结构（纯动态数组指针，杜绝 STL）
typedef struct
{
    int *lits;         // 文字数组
    int size;          // 当前有效文字数
    int original_size; // 原始文字数
    bool is_satisfied; // 是否满足标记
} Clause;

// CNF 公式物理结构
typedef struct
{
    int num_vars;    // 变元数 N
    int num_clauses; // 子句数 M
    Clause *clauses; // 子句数组
} CNFFormula;