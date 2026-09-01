#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>
#include <filesystem>
#include "config.h"
#include "types.h"
#include "cnf_parser.h"
#include "dpll_baseline.h"
#include "dpll_optimized.h"
#include "timer.h"
#include "sudoku.h"

namespace fs = std::filesystem;

#define MAX_CASE_FILES 512
struct FileList
{
    std::string filenames[MAX_CASE_FILES];
    std::string fullpaths[MAX_CASE_FILES];
    int count;
};

void run_benchmark_on_file(const char *filepath)
{
    CNFFormula *formula = parse_cnf(filepath);
    if (!formula)
        return;

    std::cout << "\n=====================================================" << std::endl;
    std::cout << "算例文件: " << filepath << std::endl;
    std::cout << "变元总数: " << formula->num_vars << " | 子句总数: " << formula->num_clauses << std::endl;
    double ratio = (formula->num_vars > 0) ? (double)formula->num_clauses / formula->num_vars : 0;
    std::cout << "子句/变元比: " << std::fixed << std::setprecision(2) << ratio << std::endl;
    std::cout << "-----------------------------------------------------" << std::endl;

    double timeout_limit = DEFAULT_TIMEOUT_MS; // 10秒超时
    double t_base = 0.0, t_opt = 0.0;

    // 1. 运行基础版
    int *assign_base = new int[formula->num_vars + 1];
    for (int i = 0; i <= formula->num_vars; ++i)
        assign_base[i] = VAL_UNASSIGNED;

    std::cout << "正在运行 [基础版 DPLL]..." << std::flush;
    SolverStatus res_base = dpll_solve_baseline_timeout(formula, assign_base, timeout_limit, &t_base);
    std::cout << " 完成!" << std::endl;

    // 2. 运行优化版
    Solver2WL *solver = create_solver_2wl(formula);

    std::cout << "正在运行 [2WL 优化版 DPLL]..." << std::flush;
    SolverStatus res_opt = dpll_solve_optimized_timeout(solver, timeout_limit, &t_opt);
    std::cout << " 完成!" << std::endl;

    // 3. 打印对比报告
    std::cout << "-----------------------------------------------------" << std::endl;
    if (res_base == STATUS_TIMEOUT)
    {
        std::cout << "[基础版] 结果: TIMEOUT (超时) | 耗时 (t) : >= " << timeout_limit << " ms" << std::endl;
    }
    else
    {
        std::cout << "[基础版] 结果: " << (res_base == STATUS_SAT ? "SATISFIABLE" : "UNSATISFIABLE")
                  << " | 耗时 (t) : " << std::fixed << std::setprecision(3) << t_base << " ms" << std::endl;
    }

    if (res_opt == STATUS_TIMEOUT)
    {
        std::cout << "[优化版] 结果: TIMEOUT (超时) | 耗时 (to): >= " << timeout_limit << " ms" << std::endl;
    }
    else
    {
        std::cout << "[优化版] 结果: " << (res_opt == STATUS_SAT ? "SATISFIABLE" : "UNSATISFIABLE")
                  << " | 耗时 (to): " << std::fixed << std::setprecision(3) << t_opt << " ms" << std::endl;
    }

    // 4. 优化率计算 (若基础版超时，计算理论下限)
    if (res_base == STATUS_TIMEOUT && res_opt != STATUS_TIMEOUT)
    {
        double lower_bound_rate = ((timeout_limit - t_opt) / timeout_limit) * 100.0;
        std::cout << ">>> 性能优化率: >= " << std::fixed << std::setprecision(2) << lower_bound_rate << " % (保守估计下限) <<<" << std::endl;
    }
    else if (res_base != STATUS_TIMEOUT && t_base > 0)
    {
        double rate = ((t_base - t_opt) / t_base) * 100.0;
        std::cout << ">>> 性能优化率: " << std::fixed << std::setprecision(2) << rate << " % <<<" << std::endl;
    }

    // 5. 按照规范导出 .res (s 1 / s 0 / s -1)
    save_solution_to_res(filepath, (int)res_opt, solver->assignment, formula->num_vars, t_opt);
    std::cout << "=====================================================" << std::endl;

    delete[] assign_base;
    free_solver_2wl(solver);
    free_cnf(formula);
}

void select_and_run_cnf()
{
    std::string target_dir = "cnf_case";
    if (!fs::exists(target_dir))
    {
        target_dir = "../cnf_case";
    }

    if (!fs::exists(target_dir) || !fs::is_directory(target_dir))
    {
        std::cout << ">> 未检测到 cnf_case 文件夹，请输入自定义路径: ";
        std::string custom_path;
        std::cin >> custom_path;
        run_benchmark_on_file(custom_path.c_str());
        return;
    }

    FileList fl;
    fl.count = 0;

    for (const auto &entry : fs::directory_iterator(target_dir))
    {
        if (entry.is_regular_file() && entry.path().extension() == ".cnf")
        {
            if (fl.count < MAX_CASE_FILES)
            {
                fl.filenames[fl.count] = entry.path().filename().string();
                fl.fullpaths[fl.count] = entry.path().string();
                fl.count++;
            }
        }
    }

    if (fl.count == 0)
    {
        std::cout << ">> 目录 [" << target_dir << "] 下没有找到任何 .cnf 文件！" << std::endl;
        return;
    }

    std::cout << "\n================ 发现的 CNF 测试算例 ================" << std::endl;
    for (int i = 0; i < fl.count; ++i)
    {
        std::cout << " [" << std::setw(2) << (i + 1) << "] " << fl.filenames[i] << std::endl;
    }
    std::cout << " [ 0] 手动输入其他文件路径" << std::endl;
    std::cout << "=====================================================" << std::endl;
    std::cout << "请输入算例序号 [1-" << fl.count << "] (或按 0 手动输入): ";

    int choice;
    if (!(std::cin >> choice))
    {
        std::cin.clear();
        std::cin.ignore(10000, '\n');
        return;
    }

    if (choice >= 1 && choice <= fl.count)
    {
        run_benchmark_on_file(fl.fullpaths[choice - 1].c_str());
    }
    else
    {
        std::cout << "请输入自定义 .cnf 文件完整路径: ";
        std::string custom_path;
        std::cin >> custom_path;
        run_benchmark_on_file(custom_path.c_str());
    }
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
        std::cout << " [1] 选择 CNF 算例测试并导出 .res 文件 (自动扫描)" << std::endl;
        std::cout << " [2] 星形数独游戏 (自动出题 / 交互填数 / SAT求解)" << std::endl;
        std::cout << " [0] 退出系统" << std::endl;
        std::cout << "=====================================================" << std::endl;
        std::cout << "请选择操作 [0-2]: ";

        int choice;
        if (!(std::cin >> choice) || choice == 0)
            break;

        if (choice == 1)
        {
            select_and_run_cnf();
        }
        else if (choice == 2)
        {
            play_sudoku_interactive();
        }
    }

    std::cout << "\n>> 感谢使用，程序已退出。" << std::endl;
    return 0;
}