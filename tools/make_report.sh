#!/bin/bash
# 从 test_logs2 日志解析生成测试结果汇总表 (CSV 供报告使用)
LOGDIR="d:/program/2026秋-程序设计综合课程设计任务及指导学生包/SAT_solver/test_logs2"
cd "$LOGDIR" || exit 1

echo "算例名|变元数|子句数|子句/变元比|求解结果|基础版t(ms)|优化版to(ms)|优化率"
for log in *.log; do
    name=$(basename "$log" .log | sed 's|^SAT测试备选算例_||; s|_|/|g')
    vars=$(grep -aoE "变元总数: [0-9]+" "$log" | head -1 | grep -oE "[0-9]+")
    clauses=$(grep -aoE "子句总数: [0-9]+" "$log" | head -1 | grep -oE "[0-9]+")
    ratio=$(grep -aoE "子句/变元比: [0-9.]+" "$log" | head -1 | grep -oE "[0-9.]+")
    # 基础版
    tb=$(grep -aoE "\[基础版\] 结果: [^|]+\| 耗时 \(t\) : [0-9.>= ]+ms" "$log" | head -1)
    res_b=$(echo "$tb" | grep -oE "SATISFIABLE|UNSATISFIABLE|TIMEOUT")
    tb_ms=$(echo "$tb" | grep -oE "[0-9.]+ ms" | grep -oE "[0-9.]+")
    [ -z "$tb_ms" ] && tb_ms=$(echo "$tb" | grep -oE ">= [0-9]+" | grep -oE "[0-9]+")
    # 优化版
    to=$(grep -aoE "\[优化版\] 结果: [^|]+\| 耗时 \(to\): [0-9.>= ]+ms" "$log" | head -1)
    res_o=$(echo "$to" | grep -oE "SATISFIABLE|UNSATISFIABLE|TIMEOUT")
    to_ms=$(echo "$to" | grep -oE "[0-9.]+ ms" | grep -oE "[0-9.]+")
    [ -z "$to_ms" ] && to_ms=$(echo "$to" | grep -oE ">= [0-9]+" | grep -oE "[0-9]+")
    # 优化率
    rate=$(grep -aoE "性能优化率: >= [0-9.]+|性能优化率: [0-9.]+" "$log" | head -1 | grep -oE "[0-9.]+")
    [ -n "$(grep -aoE "性能优化率: >=" "$log" | head -1)" ] && rate=">=$rate"
    # 结果映射
    case "$res_o" in
        SATISFIABLE) result="满足";;
        UNSATISFIABLE) result="不满足";;
        TIMEOUT) result="不确定(超时)";;
        *) result="?";;
    esac
    # 基础版超时标记
    if [ "$res_b" = "TIMEOUT" ]; then tb_ms=">${tb_ms}(超时)"; fi
    if [ "$res_o" = "TIMEOUT" ]; then to_ms=">${to_ms}(超时)"; rate="—"; fi
    echo "$name|$vars|$clauses|$ratio|$result|$tb_ms|$to_ms|$rate"
done
