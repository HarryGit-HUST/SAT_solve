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

// CDCL 求解器完整状态机
struct SolverCDCL {
    int num_vars;
    int num_clauses;
    
    // 子句动态数据库（支持动态学习子句扩容）
    Clause** clauses;
    int clauses_size;
    int clauses_cap;
    
    CDCL_WatchList* watches;
    
    // 赋值与蕴涵图跟踪
    int* assignment;     // 变元真值: -1, 0, 1
    int* level;          // 变元被赋值时的决策层级
    int* reason;         // 驱动该变元赋值的原因子句下标 (-1 表示决策变元)
    int* saved_phase;    // 极性记忆 (Phase Saving)
    
    // 时间线栈
    int* trail;
    int* trail_lim;      // 记录各决策层的起始下标
    int trail_top;
    int qhead;
    int decision_level;
    
    // VSIDS 动态启发式
    double* activity;    // 变元活跃度分数
    double var_inc;      // 活跃度增量
    double var_decay;    // 衰减因子 (通常 0.95)
    
    // 1-UIP 冲突分析辅助标记数组
    bool* seen;
    
    // Luby 重启控制
    int conflicts;
    int restart_inc;
    int luby_u;
    int luby_v;
};

// 接口
SolverCDCL* create_solver_cdcl(const CNFFormula* formula);
void free_solver_cdcl(SolverCDCL* solver);
SolverStatus cdcl_solve_timeout(SolverCDCL* solver, double timeout_ms, double* elapsed_ms);