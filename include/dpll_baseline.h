#pragma once

#include "types.h"
#include "config.h"
#include "timer.h"

// 基础版 DPLL（带超时控制）
SolverStatus dpll_solve_baseline_timeout(const CNFFormula *formula, int *assignment, double timeout_ms, double *elapsed_ms);