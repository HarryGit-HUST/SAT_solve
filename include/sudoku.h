#pragma once

#include "types.h"
#include "cnf_parser.h"
#include "dpll_optimized.h"

struct SudokuBoard
{
    int grid[9][9];
};

int pos_to_var(int r, int c, int v);
void var_to_pos(int var, int *r, int *c, int *v);
bool is_asterisk_cell(int r, int c);

CNFFormula *sudoku_to_cnf(const SudokuBoard *board);
bool cnf_to_sudoku(const int *assignment, SudokuBoard *board);

SudokuBoard generate_asterisk_sudoku(int holes);
bool solve_sudoku_board(SudokuBoard *board, double *elapsed_ms);
void print_sudoku_board(const SudokuBoard *board);
void play_sudoku_interactive();