#include "cnf_parser.h"
#include <iostream>
#include <fstream>
#include <string>
#include <iomanip>

#define MAX_LITS_PER_CLAUSE 65536

CNFFormula *parse_cnf(const char *filepath)
{
    std::ifstream fin(filepath);
    if (!fin.is_open())
    {
        std::cerr << ">> [错误] 无法打开文件: " << filepath << std::endl;
        return nullptr;
    }

    char ch;
    while (fin >> std::ws && fin.peek() == 'c')
    {
        std::string comment_line;
        std::getline(fin, comment_line);
    }

    std::string format;
    int num_vars = 0, num_clauses = 0;
    if (!(fin >> ch >> format >> num_vars >> num_clauses) || ch != 'p' || format != "cnf")
    {
        std::cerr << ">> [错误] CNF 头部格式错误 (缺少 p cnf)" << std::endl;
        fin.close();
        return nullptr;
    }

    CNFFormula *formula = new CNFFormula();
    formula->num_vars = num_vars;
    formula->num_clauses = num_clauses;
    formula->clauses = new Clause[num_clauses];

    static int temp_lits[MAX_LITS_PER_CLAUSE];
    int clause_idx = 0;

    while (clause_idx < num_clauses)
    {
        if (fin >> std::ws && fin.peek() == 'c')
        {
            std::string comment_line;
            std::getline(fin, comment_line);
            continue;
        }

        int lit_count = 0;
        int lit;
        while (fin >> lit && lit != 0)
        {
            if (lit_count < MAX_LITS_PER_CLAUSE)
            {
                temp_lits[lit_count++] = lit;
            }
        }

        formula->clauses[clause_idx].size = lit_count;
        formula->clauses[clause_idx].original_size = lit_count;
        formula->clauses[clause_idx].is_satisfied = false;
        formula->clauses[clause_idx].lits = new int[lit_count];

        for (int i = 0; i < lit_count; ++i)
        {
            formula->clauses[clause_idx].lits[i] = temp_lits[i];
        }
        clause_idx++;
    }

    fin.close();
    return formula;
}

void free_cnf(CNFFormula *formula)
{
    if (!formula)
        return;
    if (formula->clauses)
    {
        for (int i = 0; i < formula->num_clauses; ++i)
        {
            if (formula->clauses[i].lits)
            {
                delete[] formula->clauses[i].lits;
                formula->clauses[i].lits = nullptr;
            }
        }
        delete[] formula->clauses;
        formula->clauses = nullptr;
    }
    delete formula;
}

void print_and_verify_cnf(const CNFFormula *formula, std::ostream &out)
{
    if (!formula)
        return;
    out << "=== CNF 内部物理结构校验输出 ===" << std::endl;
    out << "变元数: " << formula->num_vars << ", 子句数: " << formula->num_clauses << std::endl;
    for (int i = 0; i < formula->num_clauses; ++i)
    {
        out << "子句 [" << i + 1 << "] (长度=" << formula->clauses[i].size << "): ";
        for (int j = 0; j < formula->clauses[i].size; ++j)
        {
            out << formula->clauses[i].lits[j] << " ";
        }
        out << "0" << std::endl;
    }
    out << "===============================" << std::endl;
}

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

    // 输出 s 结果: 1 (SAT), 0 (UNSAT), -1 (Timeout)
    fout << "s " << status << "\n";

    // 仅在 SAT 时输出赋值序列
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

    fout << "t " << std::fixed << std::setprecision(2) << elapsed_ms << "\n";
    fout.close();
    std::cout << ">> 结果已成功保存到: " << res_path << std::endl;
}