#!/bin/bash
# 从 test_logs4 日志生成最终三引擎报告表 (markdown 格式)
LOGDIR="d:/program/2026秋-程序设计综合课程设计任务及指导学生包/SAT_solver/test_logs4"
cd "$LOGDIR" || exit 1

# 需要输出的行: 算例日志名
LIST="
SAT测试备选算例_基准算例_功能测试_sat-20
SAT测试备选算例_基准算例_功能测试_unsat-5cnf-30
SAT测试备选算例_满足算例_S_problem2-50
SAT测试备选算例_满足算例_S_problem3-100
SAT测试备选算例_满足算例_S_problem9-100
SAT测试备选算例_满足算例_S_problem11-100
SAT测试备选算例_其它可供选择使用的算例_tst_CBS_k3_n100_m403_b10_0
SAT测试备选算例_不满足算例_tst_v10_c100
SAT测试备选算例_不满足算例_u-problem7-50
SAT测试备选算例_不满足算例_u-5cnf_3900_3900_060.shuffled-60
SAT测试备选算例_基准算例_性能测试_ais10
SAT测试备选算例_基准算例_性能测试_sud00009
SAT测试备选算例_满足算例_M_problem5-200
SAT测试备选算例_满足算例_M_problem12-200
SAT测试备选算例_满足算例_M_tst_v200_c210
SAT测试备选算例_满足算例_M_bart17.shuffled-231
SAT测试备选算例_满足算例_M_sud00001
SAT测试备选算例_满足算例_M_sud00021
SAT测试备选算例_满足算例_M_sud00082
SAT测试备选算例_满足算例_L_ec-iso-ukn009.shuffled-as.sat05-3632-1584
SAT测试备选算例_满足算例_L_eh-dp04s04.shuffled-1075
SAT测试备选算例_不满足算例_u-dp04u03.shuffled-825
SAT测试备选算例_不满足算例_gt-030.shuffled-as.sat05-1295
SAT测试备选算例_不满足算例_eu-rand_net60-25-10.shuffled-3000
SAT测试备选算例_其它可供选择使用的算例_fla-600-1
SAT测试备选算例_其它可供选择使用的算例_fla-600-2
SAT测试备选算例_其它可供选择使用的算例_fla-600-3
SAT测试备选算例_其它可供选择使用的算例_fla-600-4
"

echo "| 算例名 | 变元数 | 子句/变元比 | 结果 | t(基础版) ms | to-2wl ms | to-cdcl ms | 2WL优化率 | CDCL优化率 |"
echo "|---|---|---|---|---|---|---|---|---|"

for tag in $LIST; do
    log=$(ls "$tag"*.log 2>/dev/null | head -1)
    [ -z "$log" ] && continue
    name=$(basename "$log" .log | sed 's|^SAT测试备选算例_||; s|\.cnf$||')
    vars=$(grep -aoE "变元总数: [0-9]+" "$log" | head -1 | grep -oE "[0-9]+")
    ratio=$(grep -aoE "子句/变元比: [0-9.]+" "$log" | head -1 | grep -oE "[0-9.]+")
    l1=$(grep -aoE "\(1\) 基础版 DPLL ?: ?(SATISFIABLE|UNSATISFIABLE|超时).*" "$log" | head -1)
    l2=$(grep -aoE "\(2\) 2WL\+JW优化版: ?(SATISFIABLE|UNSATISFIABLE|超时).*" "$log" | head -1)
    l3=$(grep -aoE "\(3\) 顶配 CDCL版 ?: ?(SATISFIABLE|UNSATISFIABLE|超时).*" "$log" | head -1)
    r1=$(echo "$l1" | grep -oE "SATISFIABLE|UNSATISFIABLE|超时")
    r2=$(echo "$l2" | grep -oE "SATISFIABLE|UNSATISFIABLE|超时")
    r3=$(echo "$l3" | grep -oE "SATISFIABLE|UNSATISFIABLE|超时")
    t1=$(echo "$l1" | grep -oE "[0-9.]+ ms" | grep -oE "[0-9.]+")
    t2=$(echo "$l2" | grep -oE "[0-9.]+ ms" | grep -oE "[0-9.]+")
    t3=$(echo "$l3" | grep -oE "[0-9.]+ ms" | grep -oE "[0-9.]+")
    [ "$r1" = "超时" ] && t1=">10000"
    [ "$r2" = "超时" ] && t2=">10000"
    [ "$r3" = "超时" ] && t3=">10000"
    rate2=$(grep -aoE "2WL优化率.*" "$log" | head -1 | grep -oE "[0-9.]+" | tail -1)
    grep -a "2WL优化率" "$log" | head -1 | grep -q ">= " && rate2="≥$rate2"
    rate3=$(grep -aoE "CDCL优化率.*" "$log" | head -1 | grep -oE "[0-9.]+" | tail -1)
    grep -a "CDCL优化率" "$log" | head -1 | grep -q ">= " && rate3="≥$rate3"
    [ -z "$rate2" ] && rate2="—"
    [ -z "$rate3" ] && rate3="—"
    case "$r3" in
        SATISFIABLE) res="满足";;
        UNSATISFIABLE) res="不满足";;
        超时) res="不确定";;
    esac
    echo "| $name | $vars | $ratio | $res | $t1 | $t2 | $t3 | $rate2% | $rate3% |"
done
