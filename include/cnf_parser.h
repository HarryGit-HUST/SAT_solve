#pragma once

#include "types.h"
#include <iostream>
#include <fstream>
#include <string>

// 解析 CNF 文件并构建公式内存
CNFFormula *parse_cnf(const char *filepath);

// 释放 CNF 内存
void free_cnf(CNFFormula *formula);

// 需求⑵：公式内部表示验证输出
void print_and_verify_cnf(const CNFFormula *formula, std::ostream &out);

// 规范保存 .res 结果文件 (s / v / t)
void save_solution_to_res(const char *cnf_filepath, int status, const int *assignment, int num_vars, double elapsed_ms);