#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>
#include"cnf_parser.h"

// 假设单条子句不超过 65536
#define MAX_LITS_PER_CLAUSE 65536

CNFFormula *parse_cnf(const char *filepath)
{
    std::ifstream fin(filepath);
    if (!fin.is_open())
    {
        std::cerr << "Error: 无法打开文件 " << filepath << std::endl;
        return nullptr;
    }

    char ch;
    // 1. 跳c
    while (fin >> std::ws && fin.peek() == 'c')
    {
        std::string comment_line;
        std::getline(fin, comment_line); // 舍弃一整行注释
    }

    // 2. 读取头信息: "p cnf <vars> <clauses>"
    std::string format;
    int num_vars = 0, num_clauses = 0;

    if (!(fin >> ch >> format >> num_vars >> num_clauses) || ch != 'p' || format != "cnf")
    {
        std::cerr << "Error: CNF 文件头格式错误 (缺少 p cnf)" << std::endl;
        fin.close();
        return nullptr;
    }

    // 3. 【核心：开始分配内存】
    // (a) 分配整个 CNF 公式结构体
    CNFFormula *formula = new CNFFormula();
    formula->num_vars = num_vars;
    formula->num_clauses = num_clauses;

    // (b) 一次性开出 M 个子句的连续空间
    formula->clauses = new Clause[num_clauses];

    // 用于单条子句暂存文字的临时静态栈空间（避免频繁 malloc/new）
    static int temp_lits[MAX_LITS_PER_CLAUSE];

    // 4. 逐个子句读取文字（遇到 0 结束一个子句）
    int clause_idx = 0;
    while (clause_idx < num_clauses)
    {
        // 防止子句之间夹杂 'c' 注释行（某些不规范 CNF 会有）
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

        // (c) 为该子句精确开辟文字数组内存并拷贝
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
        // 1. 释放每个子句内部的文字数组
        for (int i = 0; i < formula->num_clauses; ++i)
        {
            if (formula->clauses[i].lits)
            {
                delete[] formula->clauses[i].lits;
                formula->clauses[i].lits = nullptr;
            }
        }
        // 2. 释放子句结构体数组
        delete[] formula->clauses;
        formula->clauses = nullptr;
    }

    // 3. 释放公式主体
    delete formula;
}
void print_and_verify_cnf(const CNFFormula *formula, std::ostream &out)
{
    if (!formula)
        return;

    out << "唉看输出" << std::endl;
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
    out << "-------------我是分割线------------" << std::endl;
}