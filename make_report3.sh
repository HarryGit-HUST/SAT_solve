#!/bin/bash
# 从 test_logs3 日志解析生成三引擎测试结果汇总表
LOGDIR="d:/program/2026秋-程序设计综合课程设计任务及指导学生包/SAT_solver/test_logs3"
cd "$LOGDIR" || exit 1

echo "算例名|变元数|子句数|子句/变元比|结果|基础版t(ms)|2WL版to-2wl(ms)|CDCL版to-cdcl(ms)|2WL优化率|CDCL优化率"
for log in *.log; do
    name=$(basename "$log" .log | sed 's|^SAT测试备选算例_||; s|^SAT_solver_||')
    vars=$(grep -aoE "变元总数: [0-9]+" "$log" | head -1 | grep -oE "[0-9]+")
    clauses=$(grep -aoE "子句总数: [0-9]+" "$log" | head -1 | grep -oE "[0-9]+")
    ratio=$(grep -aoE "子句/变元比: [0-9.]+" "$log" | head -1 | grep -oE "[0-9.]+")

    line1=$(grep -aE "\(1\) 基础版 DPLL" "$log" | head -1)
    line2=$(grep -aE "\(2\) 2WL\+JW优化版" "$log" | head -1)
    line3=$(grep -aE "\(3\) 顶配 CDCL版" "$log" | head -1)

    get_res() { echo "$1" | grep -oE "SATISFIABLE|UNSATISFIABLE|超时 TIMEOUT"; }
    get_ms() {
        # 匹配 "耗时(xxx): 数值 ms" 或 ">= 10000.00 ms"
        v=$(echo "$1" | grep -oE "耗时[^:]*: ?[0-9.]+ ms|>= [0-9.]+ ms" | grep -oE "[0-9.]+")
        echo "$1" | grep -qE ">= " && v=">= $v"
        echo "$v"
    }

    rb=$(get_res "$line1"); r2=$(get_res "$line2"); r3=$(get_res "$line3")
    tb=$(get_ms "$line1"); t2=$(get_ms "$line2"); t3=$(get_ms "$line3")
    rate2=$(grep -aoE "2WL优化率  ?: ?[>= ]*[0-9.]+" "$log" | head -1 | grep -oE "[0-9.]+")
    grep -aE "2WL优化率" "$log" | head -1 | grep -q ">= " && rate2=">=$rate2"
    rate3=$(grep -aoE "CDCL优化率 ?: ?[>= ]*[0-9.]+" "$log" | head -1 | grep -oE "[0-9.]+")
    grep -aE "CDCL优化率" "$log" | head -1 | grep -q ">= " && rate3=">=$rate3"

    case "$r3" in
        SATISFIABLE) result="满足";;
        UNSATISFIABLE) result="不满足";;
        超时) result="不确定(超时)";;
        *) result="?";;
    esac
    [ "$rb" = "超时 TIMEOUT" ] && tb="${tb}(超时)"
    [ "$r2" = "超时 TIMEOUT" ] && t2="${t2}(超时)"
    [ "$r3" = "超时 TIMEOUT" ] && t3="${t3}(超时)" && rate2="" && rate3=""
    # 基础版超时时的优化率为下限
    [ "$rb" = "超时 TIMEOUT" ] && rate2="≥${rate2#>=}" && rate3="≥${rate3#>=}"

    echo "$name|$vars|$clauses|$ratio|$result|$tb|$t2|$t3|$rate2|$rate3"
done
