/* 独立极简 DPLL 验证器 (与项目求解器完全无关的实现, 用于交叉验证 UNSAT 判定)
 * 用法: dpll_check <cnf文件> [时间上限秒, 默认120]
 * 输出: SAT(打印赋值并自检) / UNSAT / TIMEOUT
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int nvars, ncls, *cls_ofs, *cls_lits, *occ; /* occ: 每变元出现次数 */
static char *assign; /* 0=假 1=真 2=未赋值 */
static long long deadline_ms;

static long long msec(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static int parse(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[4096];
    int maxc = 1000000, nc = 0, maxl = 8000000;
    cls_ofs = malloc(sizeof(int) * maxc);
    cls_lits = malloc(sizeof(int) * maxl);
    int nl = 0;
    while (fgets(line, sizeof line, f)) {
        if (line[0] == 'c' || line[0] == '%' || line[0] == '\n') continue;
        if (line[0] == 'p') { sscanf(line, "p cnf %d %d", &nvars, &ncls); continue; }
        cls_ofs[nc++] = nl;
        char *tok = strtok(line, " \t\r\n");
        while (tok) {
            int l = atoi(tok);
            if (l == 0) break;
            if (nl >= maxl) { fprintf(stderr, "文字池溢出\n"); exit(2); }
            cls_lits[nl++] = l;
            tok = strtok(NULL, " \t\r\n");
        }
        if (nc >= maxc) break;
    }
    fclose(f);
    cls_ofs[nc] = nl; /* 哨兵 */
    ncls = nc;
    return 1;
}

static int propagate(void) /* 返回1冲突 */
{
    int changed = 1;
    while (changed) {
        changed = 0;
        for (int c = 0; c < ncls; c++) {
            int un = 0, last = 0, sat = 0;
            for (int k = cls_ofs[c]; k < cls_ofs[c + 1]; k++) {
                int l = cls_lits[k], v = l > 0 ? l : -l;
                char a = assign[v];
                if (a != 2) {
                    int truth = (l > 0) ? a : !a;
                    if (truth) { sat = 1; break; }
                } else { un++; last = l; }
            }
            if (sat) continue;
            if (un == 0) return 1; /* 冲突 */
            if (un == 1) {
                int v = last > 0 ? last : -last;
                assign[v] = last > 0 ? 1 : 0;
                changed = 1;
            }
        }
    }
    return 0;
}

static int solve(void) /* 1=SAT 0=UNSAT -1=TIMEOUT */
{
    if (msec() > deadline_ms) return -1;
    /* 选出现次数最多的未赋值变元 */
    int best = 0, bestc = -1;
    for (int v = 1; v <= nvars; v++) {
        if (assign[v] == 2 && occ[v] > bestc) { bestc = occ[v]; best = v; }
    }
    if (!best) {
        /* 全部赋值: 检查是否满足 */
        for (int c = 0; c < ncls; c++) {
            int sat = 0;
            for (int k = cls_ofs[c]; k < cls_ofs[c + 1]; k++) {
                int l = cls_lits[k], v = l > 0 ? l : -l;
                int truth = (l > 0) ? assign[v] : !assign[v];
                if (truth) { sat = 1; break; }
            }
            if (!sat) return 0;
        }
        return 1;
    }
    /* 快照式回溯: 保存当前赋值, 试两个分支 */
    char *saved = malloc(nvars + 1);
    memcpy(saved, assign, nvars + 1);
    for (int val = 1; val >= 0; val--) {
        assign[best] = val;
        if (!propagate()) {
            int r = solve();
            if (r != 0) { free(saved); return r; }
        }
        memcpy(assign, saved, nvars + 1); /* 恢复快照 */
    }
    free(saved);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "用法: %s <cnf> [秒]\n", argv[0]); return 2; }
    int limit = argc > 2 ? atoi(argv[2]) : 120;
    if (!parse(argv[1])) { fprintf(stderr, "解析失败\n"); return 2; }
    assign = calloc(nvars + 1, 1);
    occ = calloc(nvars + 1, sizeof(int));
    for (int c = 0; c < ncls; c++)
        for (int k = cls_ofs[c]; k < cls_ofs[c + 1]; k++) {
            int v = cls_lits[k] > 0 ? cls_lits[k] : -cls_lits[k];
            if (v <= nvars) occ[v]++;
        }
    for (int v = 1; v <= nvars; v++) assign[v] = 2;
    deadline_ms = msec() + (long long)limit * 1000;

    long long t0 = msec();
    if (propagate()) { printf("UNSAT (根BCP矛盾) 用时%lldms\n", msec() - t0); return 0; }
    int r = solve();
    long long el = msec() - t0;
    if (r == 1) {
        /* 自检赋值 */
        int bad = 0;
        for (int c = 0; c < ncls; c++) {
            int sat = 0;
            for (int k = cls_ofs[c]; k < cls_ofs[c + 1]; k++) {
                int l = cls_lits[k], v = l > 0 ? l : -l;
                int truth = (l > 0) ? assign[v] : !assign[v];
                if (truth) { sat = 1; break; }
            }
            if (!sat) bad++;
        }
        printf("SAT (自检: %d条子句不满足) 用时%lldms\n", bad, el);
    } else if (r == 0) {
        printf("UNSAT 用时%lldms\n", el);
    } else {
        printf("TIMEOUT 用时%lldms\n", el);
    }
    return 0;
}
