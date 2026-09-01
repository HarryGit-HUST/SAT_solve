#include "sudoku.h"
#include <iostream>
#include <iomanip>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include "timer.h"

// 星形数独 9 个特定星形坐标 (0-indexed: 行 0~8, 列 0~8)
const int ASTERISK_COORDS[9][2] = {
    {1, 4}, {2, 2}, {2, 6}, {4, 1}, {4, 4}, {4, 7}, {6, 2}, {6, 6}, {7, 4}};

bool is_asterisk_cell(int r, int c)
{
    for (int i = 0; i < 9; ++i)
    {
        if (ASTERISK_COORDS[i][0] == r && ASTERISK_COORDS[i][1] == c)
            return true;
    }
    return false;
}

int pos_to_var(int r, int c, int v)
{
    return r * 81 + c * 9 + v;
}

void var_to_pos(int var, int *r, int *c, int *v)
{
    var -= 1;
    *r = var / 81;
    *c = (var % 81) / 9;
    *v = (var % 9) + 1;
}

// 动态构建 CNF 子句
static void add_clause_to_cnf(CNFFormula *f, int *c_idx, int *lit_buf, int size)
{
    Clause *c = &f->clauses[*c_idx];
    c->size = size;
    c->original_size = size;
    c->is_satisfied = false;
    c->lits = new int[size];
    for (int i = 0; i < size; ++i)
    {
        c->lits[i] = lit_buf[i];
    }
    (*c_idx)++;
}

// 核心：数独规则转化为 CNF 公式
CNFFormula *sudoku_to_cnf(const SudokuBoard *board)
{
    CNFFormula *formula = new CNFFormula();
    formula->num_vars = 729;

    // 计算总子句数：基础约束 11988 条 + 星形约束 333 条 + 提示数条数
    int clue_count = 0;
    for (int r = 0; r < 9; ++r)
    {
        for (int c = 0; c < 9; ++c)
        {
            if (board->grid[r][c] > 0)
                clue_count++;
        }
    }

    int max_clauses = 12000 + 350 + clue_count;
    formula->clauses = new Clause[max_clauses];
    int c_idx = 0;
    int buf[10];

    // 1. 单元格约束 (At-least-1 & At-most-1)
    for (int r = 0; r < 9; ++r)
    {
        for (int c = 0; c < 9; ++c)
        {
            for (int v = 1; v <= 9; ++v)
                buf[v - 1] = pos_to_var(r, c, v);
            add_clause_to_cnf(formula, &c_idx, buf, 9);

            for (int v1 = 1; v1 <= 8; ++v1)
            {
                for (int v2 = v1 + 1; v2 <= 9; ++v2)
                {
                    buf[0] = -pos_to_var(r, c, v1);
                    buf[1] = -pos_to_var(r, c, v2);
                    add_clause_to_cnf(formula, &c_idx, buf, 2);
                }
            }
        }
    }

    // 2. 行约束
    for (int r = 0; r < 9; ++r)
    {
        for (int v = 1; v <= 9; ++v)
        {
            for (int c = 0; c < 9; ++c)
                buf[c] = pos_to_var(r, c, v);
            add_clause_to_cnf(formula, &c_idx, buf, 9);

            for (int c1 = 0; c1 < 8; ++c1)
            {
                for (int c2 = c1 + 1; c2 < 9; ++c2)
                {
                    buf[0] = -pos_to_var(r, c1, v);
                    buf[1] = -pos_to_var(r, c2, v);
                    add_clause_to_cnf(formula, &c_idx, buf, 2);
                }
            }
        }
    }

    // 3. 列约束
    for (int c = 0; c < 9; ++c)
    {
        for (int v = 1; v <= 9; ++v)
        {
            for (int r = 0; r < 9; ++r)
                buf[r] = pos_to_var(r, c, v);
            add_clause_to_cnf(formula, &c_idx, buf, 9);

            for (int r1 = 0; r1 < 8; ++r1)
            {
                for (int r2 = r1 + 1; r2 < 9; ++r2)
                {
                    buf[0] = -pos_to_var(r1, c, v);
                    buf[1] = -pos_to_var(r2, c, v);
                    add_clause_to_cnf(formula, &c_idx, buf, 2);
                }
            }
        }
    }

    // 4. 九宫格约束
    for (int br = 0; br < 3; ++br)
    {
        for (int bc = 0; bc < 3; ++bc)
        {
            for (int v = 1; v <= 9; ++v)
            {
                int k = 0;
                for (int dr = 0; dr < 3; ++dr)
                {
                    for (int dc = 0; dc < 3; ++dc)
                    {
                        buf[k++] = pos_to_var(br * 3 + dr, bc * 3 + dc, v);
                    }
                }
                add_clause_to_cnf(formula, &c_idx, buf, 9);

                for (int i = 0; i < 8; ++i)
                {
                    for (int j = i + 1; j < 9; ++j)
                    {
                        int buf2[2] = {-buf[i], -buf[j]};
                        add_clause_to_cnf(formula, &c_idx, buf2, 2);
                    }
                }
            }
        }
    }

    // 5. 星形区域特殊约束 (Asterisk Constraint)
    for (int v = 1; v <= 9; ++v)
    {
        for (int i = 0; i < 9; ++i)
        {
            buf[i] = pos_to_var(ASTERISK_COORDS[i][0], ASTERISK_COORDS[i][1], v);
        }
        add_clause_to_cnf(formula, &c_idx, buf, 9);

        for (int i = 0; i < 8; ++i)
        {
            for (int j = i + 1; j < 9; ++j)
            {
                int buf2[2] = {-buf[i], -buf[j]};
                add_clause_to_cnf(formula, &c_idx, buf2, 2);
            }
        }
    }

    // 6. 初始提示数约束 (单子句)
    for (int r = 0; r < 9; ++r)
    {
        for (int c = 0; c < 9; ++c)
        {
            if (board->grid[r][c] > 0)
            {
                int unit_lit = pos_to_var(r, c, board->grid[r][c]);
                add_clause_to_cnf(formula, &c_idx, &unit_lit, 1);
            }
        }
    }

    formula->num_clauses = c_idx;
    return formula;
}

// 解码求解结果
bool cnf_to_sudoku(const Solver2WL *solver, SudokuBoard *board)
{
    for (int var = 1; var <= 729; ++var)
    {
        if (solver->assignment[var] == VAL_TRUE)
        {
            int r, c, v;
            var_to_pos(var, &r, &c, &v);
            board->grid[r][c] = v;
        }
    }
    return true;
}

// 自动生成星形数独 (基于挖洞法)
SudokuBoard generate_asterisk_sudoku(int holes)
{
    SudokuBoard full_board;
    std::memset(&full_board, 0, sizeof(full_board));

    // 随机填充几个种子数字，利用 SAT 求解器瞬间生成完整合法终盘
    std::srand((unsigned)std::time(nullptr));
    for (int i = 0; i < 9; ++i)
    {
        int r = std::rand() % 9;
        int c = std::rand() % 9;
        int v = (std::rand() % 9) + 1;
        full_board.grid[r][c] = v;
    }

    CNFFormula *f = sudoku_to_cnf(&full_board);
    Solver2WL *solver = create_solver_2wl(f);
    if (dpll_solve_optimized(solver))
    {
        cnf_to_sudoku(solver, &full_board);
    }
    else
    {
        // 如果极罕见情况种子冲突，退回全空求解
        free_solver_2wl(solver);
        free_cnf(f);
        std::memset(&full_board, 0, sizeof(full_board));
        f = sudoku_to_cnf(&full_board);
        solver = create_solver_2wl(f);
        dpll_solve_optimized(solver);
        cnf_to_sudoku(solver, &full_board);
    }
    free_solver_2wl(solver);
    free_cnf(f);

    // 挖洞过程
    SudokuBoard puzzle = full_board;
    int dug = 0;
    while (dug < holes)
    {
        int r = std::rand() % 9;
        int c = std::rand() % 9;
        if (puzzle.grid[r][c] != 0)
        {
            puzzle.grid[r][c] = 0;
            dug++;
        }
    }
    return puzzle;
}

// 终端棋盘可视化打印（带 * 号高亮星形格）
void print_sudoku_board(const SudokuBoard *board)
{
    std::cout << "\n       1 2 3   4 5 6   7 8 9" << std::endl;
    std::cout << "     +-------+-------+-------+" << std::endl;
    for (int r = 0; r < 9; ++r)
    {
        std::cout << "   " << (r + 1) << " | ";
        for (int c = 0; c < 9; ++c)
        {
            bool is_ast = is_asterisk_cell(r, c);
            int val = board->grid[r][c];

            if (val == 0)
            {
                if (is_ast)
                    std::cout << "* ";
                else
                    std::cout << ". ";
            }
            else
            {
                std::cout << val << " ";
            }

            if ((c + 1) % 3 == 0)
                std::cout << "| ";
        }
        std::cout << std::endl;
        if ((r + 1) % 3 == 0)
        {
            std::cout << "     +-------+-------+-------+" << std::endl;
        }
    }
    std::cout << "   (提示: 带 * 号的格子为星形特殊约束区域)\n"
              << std::endl;
}

// SAT 求解接口
bool solve_sudoku_board(SudokuBoard *board, double *elapsed_ms)
{
    CNFFormula *formula = sudoku_to_cnf(board);
    Solver2WL *solver = create_solver_2wl(formula);

    Timer timer;
    timer.start();
    bool sat = dpll_solve_optimized(solver);
    *elapsed_ms = timer.stop();

    if (sat)
    {
        cnf_to_sudoku(solver, board);
    }

    free_solver_2wl(solver);
    free_cnf(formula);
    return sat;
}

// 交互式游戏循环
void play_sudoku_interactive()
{
    std::cout << "\n请选择游戏难度 [1-简单(挖30空), 2-中等(挖45空), 3-困难(挖55空)]: ";
    int diff = 1;
    if (!(std::cin >> diff))
        diff = 1;
    int holes = (diff == 1) ? 30 : ((diff == 2) ? 45 : 55);

    std::cout << "正在基于【挖洞法】生成合法星形数独游戏格局..." << std::endl;
    SudokuBoard board = generate_asterisk_sudoku(holes);

    while (true)
    {
        print_sudoku_board(&board);
        std::cout << "【操作指令】" << std::endl;
        std::cout << " 1. 填入数字 (格式: 行 列 数字, 如: 1 2 5)" << std::endl;
        std::cout << " 2. 一键 SAT 自动求解 (Solve via DPLL)" << std::endl;
        std::cout << " 0. 退出返回主菜单" << std::endl;
        std::cout << "请选择: ";

        int op;
        if (!(std::cin >> op) || op == 0)
            break;

        if (op == 1)
        {
            int r, c, v;
            std::cout << "输入坐标与值 (r c v): ";
            if (std::cin >> r >> c >> v && r >= 1 && r <= 9 && c >= 1 && c <= 9 && v >= 1 && v <= 9)
            {
                board.grid[r - 1][c - 1] = v;
                std::cout << ">> 填入成功！" << std::endl;
            }
            else
            {
                std::cout << ">> 输入不合法，请重新输入！" << std::endl;
            }
        }
        else if (op == 2)
        {
            double t_solve = 0.0;
            std::cout << ">> 正在将棋盘归约为 CNF 并调用 2WL 求解器..." << std::endl;
            if (solve_sudoku_board(&board, &t_solve))
            {
                std::cout << ">> 求解成功！求解耗时: " << std::fixed << std::setprecision(3) << t_solve << " ms" << std::endl;
                print_sudoku_board(&board);
            }
            else
            {
                std::cout << ">> 当前棋盘格局存在矛盾，无解 (UNSAT)！" << std::endl;
            }
            break;
        }
    }
}