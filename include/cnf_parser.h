// include/cnf_parser.h
#ifndef CNF_PARSER_H
#define CNF_PARSER_H

#include "types.h"
#include <iostream>
#include <stdio.h>

// 读取 CNF 文件并构建公式内存
CNFFormula* parse_cnf(const char* filepath);

// 释放 CNF 公式所占用的所有连续内存
void free_cnf(CNFFormula* formula);

// 需求⑵：公式内部表示验证（逐行输出子句，与输入算例比对人工验证）
void print_and_verify_cnf(const CNFFormula *formula, std::ostream &out);

// 求解结果输出保存到 .res 文件
void save_solution(const char *filepath, SolverResult res, const VarValue *assignment, int num_vars, double elapsed_ms);

#endif