
#include"cnf_parser.h"
#include"dpll_baseline.h"
#include"timer.h"

int main(int argc, char *argv[])
{
    const char *filename = (argc > 1) ? argv[1] : "test.cnf";

    // 1. 读取 CNF
    CNFFormula *formula = parse_cnf(filename);
    if (!formula)
        return 1;

    // 2. 初始化赋值表 (大小为 num_vars + 1，从下标 1 开始使用)
    int *assignment = new int[formula->num_vars + 1];
    for (int i = 0; i <= formula->num_vars; ++i)
    {
        assignment[i] = VAL_UNASSIGNED;
    }

    // 3. 计时
    Timer timer;
    timer.start();

    // 4. 执行
    bool is_sat = dpll_recursive_baseline(formula, assignment);
    double elapsed_ms = timer.stop();

    // 5. 打印
    std::cout << "\n--------------我是答案----------------" << std::endl;
    std::cout << "文件: " << filename << std::endl;
    std::cout << "结果: " << (is_sat ? "SATISFIABLE (可满足)" : "UNSATISFIABLE (不可满足)") << std::endl;
    std::cout << "耗时: " << elapsed_ms << " ms" << std::endl;

    if (is_sat)
    {
        std::cout << "满足解赋值: ";
        for (int v = 1; v <= formula->num_vars; ++v)
        {
            std::cout << (assignment[v] == VAL_TRUE ? v : -v) << " ";
        }
        std::cout << std::endl;
    }
    std::cout << "-------------------------------" << std::endl;

    // 6. 释放堆内存
    delete[] assignment;
    free_cnf(formula);

    return 0;
}