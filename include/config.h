#pragma once

// 默认超时阈值 (10000 毫秒 = 10 秒)
#define DEFAULT_TIMEOUT_MS 10000.0

// 检查超时的频率掩码 (每 2048 次决策检查一次系统时钟，开销极低)
#define TIMEOUT_CHECK_MASK 2047