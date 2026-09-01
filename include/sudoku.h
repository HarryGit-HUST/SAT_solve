#pragma once

#include "types.h"
#include "cnf_parser.h"
#include "dpll_optimized.h"

// 星形数独棋盘结构体 (0 表示空格，1~9 表示填入的数字)
struct SudokuBoard
{
    int grid[9][9];
};

// 坐标映射与工具函数
int pos_to_var(int r, int c, int v);
void var_to_pos(int var, int *r, int *c, int *v);
bool is_asterisk_cell(int r, int c);

// 棋盘与 CNF 转换接口
CNFFormula *sudoku_to_cnf(const SudokuBoard *board);
bool cnf_to_sudoku(const Solver2WL *solver, SudokuBoard *board);

// 自动生成与求解
SudokuBoard generate_asterisk_sudoku(int holes);
bool solve_sudoku_board(SudokuBoard *board, double *elapsed_ms);

// 终端可视化与交互展示
void print_sudoku_board(const SudokuBoard *board);
void play_sudoku_interactive();