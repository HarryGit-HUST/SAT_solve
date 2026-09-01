#include <iostream>
#include <iomanip>
#include <cstring>
#include "cnf_parser.h"
#include "dpll_baseline.h"
#include "dpll_optimized.h"
#include "timer.h"

int main(int argc, char *argv[])
{
    const char *filename = (argc > 1) ? argv[1] : "test.cnf";

    std::cout << "=====================================================" << std::endl;
    std::cout << "        不要用我解鸽笼和异或展开等等          " << std::endl;
    std::cout << "=====================================================" << std::endl;
    std::cout << "算例: " << filename << std::endl;

    // 1. 读取 CNF 文件
    CNFFormula *formula = parse_cnf(filename);
    if (!formula)
        return 1;

    std::cout << "变元总数: " << formula->num_vars << " | 子句总数: " << formula->num_clauses << std::endl;
    std::cout << "-----------------------------------------------------" << std::endl;

    Timer timer;
    double t_base = 0.0;
    double t_opt = 0.0;
    bool res_base = false;
    bool res_opt = false;
    // 3. 运行 2WL 优化版本 DPLL (Optimized)
    {
        Solver2WL *solver = create_solver_2wl(formula);

        timer.start();
        res_opt = dpll_solve_optimized(solver);
        t_opt = timer.stop();

        free_solver_2wl(solver);
    }

    std::cout << "[v2] 结果: " << (res_opt ? "SAT" : "UNSAT")
              << " | 耗时 (to): " << std::fixed << std::setprecision(3) << t_opt << " ms" << std::endl;
    std::cout << "-----------------------------------------------------" << std::endl;

    // 2. 运行基础版本 DPLL (Baseline)
    {
        int *assignment = new int[formula->num_vars + 1];
        for (int i = 0; i <= formula->num_vars; ++i)
            assignment[i] = VAL_UNASSIGNED;

        timer.start();
        res_base = dpll_recursive_baseline(formula, assignment);
        t_base = timer.stop();

        delete[] assignment;
    }

    std::cout << "[v1] 结果: " << (res_base ? "SAT" : "UNSAT")
              << " | 耗时 (t) : " << std::fixed << std::setprecision(3) << t_base << " ms" << std::endl;



    // 4. 计算优化率 [(t - to) / t] * 100%
    if (t_base > 0.0)
    {
        double opt_rate = ((t_base - t_opt) / t_base) * 100.0;
        std::cout << ">>> 提升率: " << std::fixed << std::setprecision(2) << opt_rate << " % <<<" << std::endl;
    }
    else
    {
        std::cout << ">>> 基础耗时过短 (<0.001ms)，无法计算有效优化率 <<<" << std::endl;
    }
    std::cout << "=====================================================" << std::endl;

    free_cnf(formula);
    return 0;
}