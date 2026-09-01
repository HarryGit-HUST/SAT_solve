#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>
#include <filesystem> // C++17 跨平台文件系统库
#include "cnf_parser.h"
#include "dpll_baseline.h"
#include "dpll_optimized.h"
#include "timer.h"
#include "sudoku.h"

namespace fs = std::filesystem;

// 存储文件列表的手动动态数组结构（严格遵循不用 STL vector 的原则）
#define MAX_CASE_FILES 512
struct FileList
{
    std::string filenames[MAX_CASE_FILES];
    std::string fullpaths[MAX_CASE_FILES];
    int count;
};

// 规范保存 .res 结果文件
void save_solution_to_res(const char *cnf_filepath, int status, const int *assignment, int num_vars, double elapsed_ms)
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

    // s 结果: 1 (SAT), 0 (UNSAT), -1 (Timeout/Unknown)
    fout << "s " << status << "\n";

    // v 满足解赋值
    if (status == 1 && assignment != nullptr)
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

    // t 求解耗时 (ms)
    fout << "t " << std::fixed << std::setprecision(2) << elapsed_ms << "\n";
    fout.close();
    std::cout << ">> 结果已成功保存到: " << res_path << std::endl;
}

// 核心求解与性能对比测试入口
void run_benchmark_on_file(const char *filepath)
{
    CNFFormula *formula = parse_cnf(filepath);
    if (!formula)
    {
        std::cerr << ">> [错误] 无法解析 CNF 文件: " << filepath << std::endl;
        return;
    }

    std::cout << "\n=====================================================" << std::endl;
    std::cout << "算例文件: " << filepath << std::endl;
    std::cout << "变元总数: " << formula->num_vars << " | 子句总数: " << formula->num_clauses << std::endl;
    double ratio = (formula->num_vars > 0) ? (double)formula->num_clauses / formula->num_vars : 0;
    std::cout << "子句/变元比: " << std::fixed << std::setprecision(2) << ratio << std::endl;
    std::cout << "-----------------------------------------------------" << std::endl;

    Timer timer;
    double t_base = 0.0, t_opt = 0.0;
    bool res_base = false, res_opt = false;

    // 1. 基础版本 DPLL
    int *assign_base = new int[formula->num_vars + 1];
    for (int i = 0; i <= formula->num_vars; ++i)
        assign_base[i] = VAL_UNASSIGNED;

    std::cout << "正在运行 [基础版 DPLL]..." << std::flush;
    timer.start();
    res_base = dpll_recursive_baseline(formula, assign_base);
    t_base = timer.stop();
    std::cout << " 完成!" << std::endl;

    // 2. 优化版本 2WL DPLL
    Solver2WL *solver = create_solver_2wl(formula);

    std::cout << "正在运行 [2WL 优化版 DPLL]..." << std::flush;
    timer.start();
    res_opt = dpll_solve_optimized(solver);
    t_opt = timer.stop();
    std::cout << " 完成!" << std::endl;

    // 3. 打印对比报告
    std::cout << "-----------------------------------------------------" << std::endl;
    std::cout << "[基础版] 结果: " << (res_base ? "SATISFIABLE" : "UNSATISFIABLE")
              << " | 耗时 (t) : " << std::fixed << std::setprecision(3) << t_base << " ms" << std::endl;
    std::cout << "[优化版] 结果: " << (res_opt ? "SATISFIABLE" : "UNSATISFIABLE")
              << " | 耗时 (to): " << std::fixed << std::setprecision(3) << t_opt << " ms" << std::endl;

    if (t_base > 0.0)
    {
        double rate = ((t_base - t_opt) / t_base) * 100.0;
        std::cout << ">>> 性能优化率: " << std::fixed << std::setprecision(2) << rate << " % <<<" << std::endl;
    }

    // 4. 导出 .res 文件
    save_solution_to_res(filepath, res_opt ? 1 : 0, solver->assignment, formula->num_vars, t_opt);
    std::cout << "=====================================================" << std::endl;

    delete[] assign_base;
    free_solver_2wl(solver);
    free_cnf(formula);
}

// 自动扫描 cnf_case 目录并呈现菜单选择
void select_and_run_cnf()
{
    // 候选目录：兼容从根目录运行或从 build 目录运行
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

    // 扫描目录下的所有 .cnf 文件
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

    // 格式化输出文件列表
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
    // 支持命令行直接跟参数运行：./sat_solver ../cnf_case/sud00001.cnf
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