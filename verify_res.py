#!/usr/bin/env python3
"""独立验证 .res 文件:
- s 1: 检查 v 行赋值是否真的满足对应 cnf (逐子句验证)
- s 0: 无法独立验证, 仅报告
- s -1: 报告超时
"""
import sys, os, re, glob

ROOT = r"d:\program\2026秋-程序设计综合课程设计任务及指导学生包"

def parse_cnf(path):
    vars_, clauses = None, None
    with open(path, encoding="utf-8", errors="ignore") as f:
        content = f.read()
    cls = []
    for line in content.splitlines():
        line = line.strip()
        if not line or line.startswith(("c", "%", "0")):
            continue
        if line.startswith("p"):
            parts = line.split()
            vars_, clauses = int(parts[2]), int(parts[3])
            continue
        lits = [int(x) for x in line.split() if x != "0"]
        if lits:
            cls.append(lits)
    return vars_, clauses, cls

def parse_res(path):
    status, assignment = None, []
    with open(path, encoding="utf-8", errors="ignore") as f:
        for line in f:
            line = line.strip()
            if line.startswith("s "):
                status = int(line[2:].strip())
            elif line.startswith("v "):
                assignment.extend(int(x) for x in line[2:].split())
    return status, assignment

def verify(cnf_path, res_path):
    vars_, clauses, cls = parse_cnf(cnf_path)
    status, assign = parse_res(res_path)
    lit_true = set(assign)
    bad = 0
    for c in cls:
        if not any(l in lit_true for l in c):
            bad += 1
    # 仅对声明为 SAT 的文件打印违反子句 (UNSAT/超时无赋值, 不适用)
    if status == 1 and bad > 0:
        shown = 0
        for c in cls:
            if not any(l in lit_true for l in c):
                print(f"    违反子句: {c}")
                shown += 1
                if shown >= 3:
                    break
    return status, vars_, clauses, len(cls), len(assign), bad

def main():
    ok, fail = 0, 0
    # 收集所有 .res 文件 (与 cnf 同目录)
    res_files = []
    for root, dirs, files in os.walk(ROOT):
        if "test_logs" in root or "SAT_solver\\build" in root:
            continue
        for f in files:
            if f.endswith(".res"):
                res_files.append(os.path.join(root, f))
    res_files.sort()
    print(f"共发现 {len(res_files)} 个 .res 文件\n")
    for rf in res_files:
        cf = rf[:-4] + ".cnf"
        if not os.path.exists(cf):
            print(f"[跳过] {rf} (无对应cnf)")
            continue
        status, v, nc, ncls, na, bad = verify(cf, rf)
        name = os.path.relpath(rf, ROOT)
        if status == 1:
            if bad == 0:
                print(f"[OK-SAT]   {name}  (变元{v} 子句{ncls} 赋值{na}个 全部满足)")
                ok += 1
            else:
                print(f"[FAIL]     {name}  s=1 但 {bad} 条子句不满足!")
                fail += 1
        elif status == 0:
            print(f"[UNSAT]    {name}  (变元{v} 子句{ncls} 声明不满足, 无法独立验证)")
        elif status == -1:
            print(f"[TIMEOUT]  {name}  (变元{v} 子句{ncls} 超时)")
        else:
            print(f"[??]       {name}  s={status}")
    print(f"\nSAT验证: {ok} 个通过, {fail} 个失败")

if __name__ == "__main__":
    main()
