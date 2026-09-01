#include <iostream>
#include <iomanip>
#include "cnf_parser.h"
#include "dpll_baseline.h"
#include "dpll_optimized.h"
#include "timer.h"
#include "sudoku.h"

// 声明保存函数
void save_solution_to_res(const char *cnf_filepath, int status, const int *assignment, int num_vars, double elapsed_ms);

void run_benchmark_on_file(const char *filepath)
{
    CNFFormula *formula = parse_cnf(filepath);
    if (!formula)
        return;

    std::cout << "\n=====================================================" << std::endl;
    std::cout << "算例文件: " << filepath << std::endl;
    std::cout << "变元数: " << formula->num_vars << " | 子句数: " << formula->num_clauses << std::endl;

    Timer timer;
    double t_base = 0.0, t_opt = 0.0;
    bool res_base = false, res_opt = false;

    // 1. 基础 DPLL
    int *assign_base = new int[formula->num_vars + 1];
    for (int i = 0; i <= formula->num_vars; ++i)
        assign_base[i] = VAL_UNASSIGNED;
    timer.start();
    res_base = dpll_recursive_baseline(formula, assign_base);
    t_base = timer.stop();

    // 2. 优化 2WL DPLL
    Solver2WL *solver = create_solver_2wl(formula);
    timer.start();
    res_opt = dpll_solve_optimized(solver);
    t_opt = timer.stop();

    std::cout << "[基础版] 结果: " << (res_base ? "SAT" : "UNSAT") << " | 耗时(t) : " << std::fixed << std::setprecision(3) << t_base << " ms" << std::endl;
    std::cout << "[优化版] 结果: " << (res_opt ? "SAT" : "UNSAT") << " | 耗时(to): " << std::fixed << std::setprecision(3) << t_opt << " ms" << std::endl;

    if (t_base > 0.0)
    {
        double rate = ((t_base - t_opt) / t_base) * 100.0;
        std::cout << ">>> 性能优化率: " << std::fixed << std::setprecision(2) << rate << " % <<<" << std::endl;
    }

    // 按照规范输出 .res 文件
    save_solution_to_res(filepath, res_opt ? 1 : 0, solver->assignment, formula->num_vars, t_opt);

    delete[] assign_base;
    free_solver_2wl(solver);
    free_cnf(formula);
}

int main(int argc, char *argv[])
{
    if (argc > 1)
    {
        run_benchmark_on_file(argv[1]);
        return 0;
    }

    while (true)
    {
        std::cout << "\n=====================================================" << std::endl;
        std::cout << "           SAT 求解器与星形数独系统 (SAT-Sudoku)      " << std::endl;
        std::cout << "=====================================================" << std::endl;
        std::cout << " [1] 运行 SAT 算例基准测试与导出 .res 文件" << std::endl;
        std::cout << " [2] 星形数独游戏 (自动出题 / 交互填数 / SAT求解)" << std::endl;
        std::cout << " [0] 退出系统" << std::endl;
        std::cout << "=====================================================" << std::endl;
        std::cout << "请选择操作 [0-2]: ";

        int choice;
        if (!(std::cin >> choice) || choice == 0)
            break;

        if (choice == 1)
        {
            std::string path;
            std::cout << "请输入 .cnf 文件路径: ";
            std::cin >> path;
            run_benchmark_on_file(path.c_str());
        }
        else if (choice == 2)
        {
            play_sudoku_interactive();
        }
    }
    return 0;
}