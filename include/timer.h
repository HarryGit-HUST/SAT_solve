// timer.h
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

    double stop()
    {
        end_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed = end_time - start_time;
        return elapsed.count(); // 返回毫秒数 (ms)
    }
};