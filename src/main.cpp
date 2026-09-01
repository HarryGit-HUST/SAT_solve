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
#include "dpll_cdcl.h"
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

// 规范保存 .res 结果文件
static void save_res_file(const char *cnf_filepath, SolverStatus status, const int *assignment, int num_vars, double elapsed_ms)
{
    std::string res_path = cnf_filepath;
    size_t last_dot = res_path.find_last_of('.');
    if (last_dot != std::string::npos)
    {
        res_path = res_path.substr(0, last_dot);
    }
    res_path += ".res";

    std::ofstream fout(res_path);
    if (!fout.is_open())
        return;

    // s 结果: 1 (SAT), 0 (UNSAT), -1 (Timeout)
    fout << "s " << (int)status << "\n";

    // 仅在 SAT 且有赋值时输出 v 序列
    if (status == STATUS_SAT && assignment != nullptr)
    {
        fout << "v ";
        for (int i = 1; i <= num_vars; ++i)
        {
            if (assignment[i] == VAL_TRUE)
                fout << i << " ";
            else
                fout << -i << " ";
        }
        fout << "\n";
    }

    fout << "t " << std::fixed << std::setprecision(2) << elapsed_ms << "\n";
    fout.close();
    std::cout << ">> 结果已自动保存到: " << res_path << std::endl;
}

// 核心：三引擎基准评测
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
    double t_base = 0.0, t_2wl = 0.0, t_cdcl = 0.0;
    SolverStatus res_base = STATUS_TIMEOUT;
    SolverStatus res_2wl = STATUS_TIMEOUT;
    SolverStatus res_cdcl = STATUS_TIMEOUT;

    // 1. 引擎一：基础版 DPLL
    {
        int *assign_base = new int[formula->num_vars + 1];
        for (int i = 0; i <= formula->num_vars; ++i)
            assign_base[i] = VAL_UNASSIGNED;

        std::cout << "[1/3] 正在运行 [基础版 DPLL]..." << std::flush;
        res_base = dpll_solve_baseline_timeout(formula, assign_base, timeout_limit, &t_base);
        std::cout << " 完成!" << std::endl;

        delete[] assign_base;
    }

    // 2. 引擎二：2WL + JW 优化版
    {
        Solver2WL *solver_2wl = create_solver_2wl(formula);

        std::cout << "[2/3] 正在运行 [2WL + JW 优化版]..." << std::flush;
        res_2wl = dpll_solve_optimized_timeout(solver_2wl, timeout_limit, &t_2wl);
        std::cout << " 完成!" << std::endl;

        free_solver_2wl(solver_2wl);
    }

    // 3. 引擎三：顶配 CDCL (1-UIP + VSIDS + Luby 重启)
    SolverCDCL *solver_cdcl = create_solver_cdcl(formula);
    {
        std::cout << "[3/3] 正在运行 [顶配 CDCL 引擎]..." << std::flush;
        res_cdcl = cdcl_solve_timeout(solver_cdcl, timeout_limit, &t_cdcl);
        std::cout << " 完成!" << std::endl;
    }

    // 4. 打印三引擎对比与优化率报表
    std::cout << "-----------------------------------------------------" << std::endl;
    std::cout << "【三引擎求解报告】" << std::endl;

    // 基础版输出
    if (res_base == STATUS_TIMEOUT)
    {
        std::cout << "  (1) 基础版 DPLL : 超时 TIMEOUT | 耗时(t)    : >= " << timeout_limit << " ms" << std::endl;
    }
    else
    {
        std::cout << "  (1) 基础版 DPLL : " << (res_base == STATUS_SAT ? "SATISFIABLE" : "UNSATISFIABLE")
                  << " | 耗时(t)    : " << std::fixed << std::setprecision(3) << t_base << " ms" << std::endl;
    }

    // 2WL 优化版输出
    if (res_2wl == STATUS_TIMEOUT)
    {
        std::cout << "  (2) 2WL+JW优化版: 超时 TIMEOUT | 耗时(to-2wl): >= " << timeout_limit << " ms" << std::endl;
    }
    else
    {
        std::cout << "  (2) 2WL+JW优化版: " << (res_2wl == STATUS_SAT ? "SATISFIABLE" : "UNSATISFIABLE")
                  << " | 耗时(to-2wl): " << std::fixed << std::setprecision(3) << t_2wl << " ms" << std::endl;
    }

    // CDCL 顶配版输出
    if (res_cdcl == STATUS_TIMEOUT)
    {
        std::cout << "  (3) 顶配 CDCL版 : 超时 TIMEOUT | 耗时(to-cdcl): >= " << timeout_limit << " ms" << std::endl;
    }
    else
    {
        std::cout << "  (3) 顶配 CDCL版 : " << (res_cdcl == STATUS_SAT ? "SATISFIABLE" : "UNSATISFIABLE")
                  << " | 耗时(to-cdcl): " << std::fixed << std::setprecision(3) << t_cdcl << " ms" << std::endl;
    }
    std::cout << "-----------------------------------------------------" << std::endl;

    // 5. 优化率统计 (对比基础版)
    if (res_base == STATUS_TIMEOUT)
    {
        if (res_2wl != STATUS_TIMEOUT)
        {
            double rate_2wl = ((timeout_limit - t_2wl) / timeout_limit) * 100.0;
            std::cout << ">>> 2WL优化率  : >= " << std::fixed << std::setprecision(2) << rate_2wl << " % (保守估计)" << std::endl;
        }
        if (res_cdcl != STATUS_TIMEOUT)
        {
            double rate_cdcl = ((timeout_limit - t_cdcl) / timeout_limit) * 100.0;
            std::cout << ">>> CDCL优化率 : >= " << std::fixed << std::setprecision(2) << rate_cdcl << " % (保守估计)" << std::endl;
        }
    }
    else if (t_base > 0.0)
    {
        if (res_2wl != STATUS_TIMEOUT)
        {
            double rate_2wl = ((t_base - t_2wl) / t_base) * 100.0;
            std::cout << ">>> 2WL优化率  : " << std::fixed << std::setprecision(2) << rate_2wl << " %" << std::endl;
        }
        if (res_cdcl != STATUS_TIMEOUT)
        {
            double rate_cdcl = ((t_base - t_cdcl) / t_base) * 100.0;
            std::cout << ">>> CDCL优化率 : " << std::fixed << std::setprecision(2) << rate_cdcl << " %" << std::endl;
        }
    }

    // 6. 按照任务书规范输出 .res 文件 (优先采用最强的 CDCL 结果)
    save_res_file(filepath, res_cdcl, solver_cdcl->assignment, formula->num_vars, t_cdcl);
    std::cout << "=====================================================" << std::endl;

    free_solver_cdcl(solver_cdcl);
    free_cnf(formula);
}

// 自动扫描 cnf_case 目录
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

// 主交互总控菜单
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
        std::cout << "       SAT 求解器 (三引擎基准) 与星形数独系统          " << std::endl;
        std::cout << "=====================================================" << std::endl;
        std::cout << " [1] 三引擎性能基准测试与导出 .res (自动扫描)" << std::endl;
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