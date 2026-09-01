#pragma once

// 默认超时阈值（单位：毫秒）。可设置为 5000ms (5秒) 或 10000ms (10秒)
#define DEFAULT_TIMEOUT_MS 10000.0

// 检查超时的频率掩码 (2047 相当于每 2048 次决策检查一次时间，开销极小)
#define TIMEOUT_CHECK_MASK 2047

// 求解器返回状态码规范
enum SolverStatus
{
    STATUS_TIMEOUT = -1, // s -1 (超时未完成)
    STATUS_UNSAT = 0,    // s 0  (证明不可满足)
    STATUS_SAT = 1       // s 1  (找到可满足解)
};