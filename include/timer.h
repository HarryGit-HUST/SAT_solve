#pragma once

#include <chrono>

class Timer
{
private:
    std::chrono::high_resolution_clock::time_point start_time;
    std::chrono::high_resolution_clock::time_point end_time;

public:
    void start()
    {
        start_time = std::chrono::high_resolution_clock::now();
    }

    // 停止计时并返回总毫秒数
    double stop()
    {
        end_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed = end_time - start_time;
        return elapsed.count();
    }

    // 核心修复：不停止计时，实时获取从 start() 到当前经过的毫秒数
    double get_elapsed_ms() const
    {
        auto now = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed = now - start_time;
        return elapsed.count();
    }
};