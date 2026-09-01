#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>
#include <cstdlib>
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

// 规范保存 .res 结果文件 (t 行按任务书允许追加分支规则执行次数等统计信息)
static void save_res_file(const char *cnf_filepath, SolverStatus status, const int *assignment, int num_vars,
                          double elapsed_ms, const char *engine_name, const SolverStats *stats)
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

    fout << "t " << std::fixed << std::setprecision(2) << elapsed_ms;
    if (engine_name && stats)
    {
        fout << " | " << engine_name
             << " d=" << stats->decisions
             << " p=" << stats->propagations
             << " c=" << stats->conflicts
             << " r=" << stats->restarts;
    }
    fout << "\n";
    fout.close();
    std::cout << ">> 结果已自动保存到: " << res_path << std::endl;
}

// 核心：三引擎基准评测
void run_benchmark_on_file(const char *filepath, double timeout_limit)
{
    CNFFormula *formula = parse_cnf(filepath);
    if (!formula)
        return;

    std::cout << "\n=====================================================" << std::endl;
    std::cout << "算例文件: " << filepath << std::endl;
    std::cout << "变元总数: " << formula->num_vars << " | 子句总数: " << formula->num_clauses << std::endl;
    double ratio = (formula->num_vars > 0) ? (double)formula->num_clauses / formula->num_vars : 0;
    std::cout << "子句/变元比: " << std::fixed << std::setprecision(2) << ratio << std::endl;
    std::cout << "引擎超时上限: " << std::fixed << std::setprecision(0) << timeout_limit << " ms" << std::endl;
    std::cout << "-----------------------------------------------------" << std::endl;

    double t_base = 0.0, t_2wl = 0.0, t_cdcl = 0.0;
    SolverStatus res_base = STATUS_TIMEOUT;
    SolverStatus res_2wl = STATUS_TIMEOUT;
    SolverStatus res_cdcl = STATUS_TIMEOUT;
    SolverStats stats_base = {0, 0, 0, 0};

    // 1. 引擎一：基础版 DPLL
    int *assign_base = new int[formula->num_vars + 1];
    for (int i = 0; i <= formula->num_vars; ++i)
        assign_base[i] = VAL_UNASSIGNED;

    std::cout << "[1/3] 正在运行 [基础版 DPLL]..." << std::flush;
    res_base = dpll_solve_baseline_timeout(formula, assign_base, timeout_limit, &t_base, &stats_base);
    std::cout << " 完成!" << std::endl;

    // 2. 引擎二：2WL + JW 优化版 (求解器保留至 .res 写入, 便于结果回退使用)
    Solver2WL *solver_2wl = create_solver_2wl(formula);

    std::cout << "[2/3] 正在运行 [2WL + JW 优化版]..." << std::flush;
    res_2wl = dpll_solve_optimized_timeout(solver_2wl, timeout_limit, &t_2wl);
    std::cout << " 完成!" << std::endl;

    // 3. 引擎三：顶配 CDCL (1-UIP + VSIDS + Luby 重启 + LBD 子句库管理)
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

    // 6. 按照任务书规范输出 .res 文件：
    //    优先采用最强引擎的确定结果 (CDCL → 2WL → 基础版), 三个引擎全部超时才写 s -1。
    //    这样可避免出现 "2WL 已解出但 CDCL 超时 → .res 反而退化" 的情况。
    SolverStatus best = res_cdcl;
    const int *best_assign = solver_cdcl->assignment;
    const SolverStats *best_stats = &solver_cdcl->stats;
    double best_t = t_cdcl;
    const char *best_engine = "CDCL";
    if (best == STATUS_TIMEOUT && res_2wl != STATUS_TIMEOUT)
    {
        best = res_2wl;
        best_assign = solver_2wl->assignment;
        best_stats = &solver_2wl->stats;
        best_t = t_2wl;
        best_engine = "2WL";
    }
    if (best == STATUS_TIMEOUT && res_base != STATUS_TIMEOUT)
    {
        best = res_base;
        best_assign = assign_base;
        best_stats = &stats_base;
        best_t = t_base;
        best_engine = "基础版";
    }
    save_res_file(filepath, best, best_assign, formula->num_vars, best_t, best_engine, best_stats);
    std::cout << ">> .res 采用引擎: " << best_engine << std::endl;
    std::cout << "=====================================================" << std::endl;

    delete[] assign_base;
    free_solver_2wl(solver_2wl);
    free_solver_cdcl(solver_cdcl);
    free_cnf(formula);
}

// 自动扫描 cnf_case 目录（求解与验证两个入口共用）
bool scan_cnf_case(FileList &fl, std::string &target_dir)
{
    // 候选目录：兼容从根目录运行或从 build 目录运行
    target_dir = "cnf_case";
    if (!fs::exists(target_dir))
    {
        target_dir = "../cnf_case";
    }

    if (!fs::exists(target_dir) || !fs::is_directory(target_dir))
        return false;

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
    return true;
}

// 需求⑵：公式解析验证 —— 遍历内部物理结构逐行输出每个子句，与原始算例人工比对
void select_and_verify_cnf()
{
    FileList fl;
    std::string target_dir;
    if (!scan_cnf_case(fl, target_dir))
    {
        std::cout << ">> 未检测到 cnf_case 文件夹，请输入自定义路径: ";
        std::string custom_path;
        std::cin >> custom_path;
        CNFFormula *formula = parse_cnf(custom_path.c_str());
        if (formula)
        {
            print_and_verify_cnf(formula, std::cout);
            free_cnf(formula);
        }
        return;
    }

    std::cout << "\n================ 发现的 CNF 测试算例 ================" << std::endl;
    for (int i = 0; i < fl.count; ++i)
    {
        std::cout << " [" << std::setw(2) << (i + 1) << "] " << fl.filenames[i] << std::endl;
    }
    std::cout << "=====================================================" << std::endl;
    std::cout << "请输入算例序号 [1-" << fl.count << "] 进行解析验证: ";

    int choice;
    if (!(std::cin >> choice) || choice < 1 || choice > fl.count)
        return;

    CNFFormula *formula = parse_cnf(fl.fullpaths[choice - 1].c_str());
    if (formula)
    {
        print_and_verify_cnf(formula, std::cout);
        free_cnf(formula);
    }
}

// 自动扫描 cnf_case 目录
void select_and_run_cnf()
{
    FileList fl;
    std::string target_dir;
    if (!scan_cnf_case(fl, target_dir))
    {
        std::cout << ">> 未检测到 cnf_case 文件夹，请输入自定义路径: ";
        std::string custom_path;
        std::cin >> custom_path;
        run_benchmark_on_file(custom_path.c_str(), DEFAULT_TIMEOUT_MS);
        return;
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
        run_benchmark_on_file(fl.fullpaths[choice - 1].c_str(), DEFAULT_TIMEOUT_MS);
    }
    else
    {
        std::cout << "请输入自定义 .cnf 文件完整路径: ";
        std::string custom_path;
        std::cin >> custom_path;
        run_benchmark_on_file(custom_path.c_str(), DEFAULT_TIMEOUT_MS);
    }
}

// 主交互总控菜单
int main(int argc, char *argv[])
{
    if (argc > 1)
    {
        // 命令行: sat_solver.exe <cnf文件> [超时秒数]
        double limit = (argc > 2) ? std::atof(argv[2]) * 1000.0 : DEFAULT_TIMEOUT_MS;
        run_benchmark_on_file(argv[1], limit);
        return 0;
    }

    while (true)
    {
        std::cout << "\n=====================================================" << std::endl;
        std::cout << "       SAT 求解器 (三引擎基准) 与星形数独系统          " << std::endl;
        std::cout << "=====================================================" << std::endl;
        std::cout << " [1] 三引擎性能基准测试与导出 .res (自动扫描)" << std::endl;
        std::cout << " [2] 星形数独游戏 (自动出题 / 交互填数 / SAT求解)" << std::endl;
        std::cout << " [3] 公式解析验证 (输出内部子句结构人工比对)" << std::endl;
        std::cout << " [0] 退出系统" << std::endl;
        std::cout << "=====================================================" << std::endl;
        std::cout << "请选择操作 [0-3]: ";

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
        else if (choice == 3)
        {
            select_and_verify_cnf();
        }
    }

    std::cout << "\n>> 感谢使用，程序已退出。" << std::endl;
    return 0;
}