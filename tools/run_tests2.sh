#!/bin/bash
# SAT 批量测试脚本 v2：使用带超时机制的新版求解器 (build2/Release)
# 每个求解器内部自带 10s 上限，外壳再加 90s 保险
ROOT="d:/program/2026秋-程序设计综合课程设计任务及指导学生包"
EXE="$ROOT/SAT_solver/build2/Release/sat_solver.exe"
LOGDIR="$ROOT/SAT_solver/test_logs2"
mkdir -p "$LOGDIR"
SUMMARY="$LOGDIR/summary.txt"
: > "$SUMMARY"

run_one() {
    cnf="$1"
    tag=$(echo "$cnf" | sed 's|[/\\]|_|g; s|\.cnf$||')
    start=$(date +%s)
    timeout 90 "$EXE" "$cnf" > "$LOGDIR/$tag.log" 2>&1
    code=$?
    end=$(date +%s)
    wall=$((end - start))
    echo "$tag|exit=$code|wall=${wall}s" >> "$SUMMARY"
    echo "[done $((++DONE))/$TOTAL] exit=$code wall=${wall}s  $cnf"
}

cd "$ROOT"

# ---- 小型算例 ----
SMALL=(
"SAT测试备选算例/满足算例/S/problem2-50.cnf"
"SAT测试备选算例/满足算例/S/problem3-100.cnf"
"SAT测试备选算例/满足算例/S/problem6-50.cnf"
"SAT测试备选算例/满足算例/S/problem8-50.cnf"
"SAT测试备选算例/满足算例/S/problem9-100.cnf"
"SAT测试备选算例/满足算例/S/problem11-100.cnf"
"SAT测试备选算例/满足算例/S/7cnf20_90000_90000_7.shuffled-20.cnf"
"SAT测试备选算例/其它可供选择使用的算例/tst/tst_v100_c160.cnf"
"SAT测试备选算例/其它可供选择使用的算例/tst/CBS_k3_n100_m403_b10_0.cnf"
"SAT测试备选算例/其它可供选择使用的算例/tst/flat30-1.cnf"
"SAT测试备选算例/不满足算例/tst_v10_c100.cnf"
"SAT测试备选算例/不满足算例/u-problem7-50.cnf"
"SAT测试备选算例/不满足算例/u-problem10-100.cnf"
"SAT测试备选算例/不满足算例/u-5cnf_3500_3500_30f1.shuffled-30.cnf"
"SAT测试备选算例/不满足算例/u-5cnf_3900_3900_060.shuffled-60.cnf"
"SAT测试备选算例/不满足算例/u-5cnf_4300_4300_110.shuffled-110.cnf"
)

# ---- 中型算例 ----
MEDIUM=(
"SAT测试备选算例/基准算例/性能测试/ais10.cnf"
"SAT测试备选算例/基准算例/性能测试/sud00009.cnf"
"SAT测试备选算例/满足算例/M/problem5-200.cnf"
"SAT测试备选算例/满足算例/M/problem12-200.cnf"
"SAT测试备选算例/满足算例/M/tst_v200_c210.cnf"
"SAT测试备选算例/其它可供选择使用的算例/tst/tst_v200_c220.cnf"
"SAT测试备选算例/满足算例/M/bart17.shuffled-231.cnf"
"SAT测试备选算例/满足算例/M/m-mod2c-rand3bip-sat-220-3.shuffled-as.sat05-2490-311.cnf"
"SAT测试备选算例/满足算例/M/ec-mod2c-rand3bip-sat-250-2.shuffled-as.sat05-2534.cnf"
"SAT测试备选算例/满足算例/M/sud00001.cnf"
"SAT测试备选算例/满足算例/M/sud00012.cnf"
"SAT测试备选算例/满足算例/M/sud00021.cnf"
"SAT测试备选算例/满足算例/M/sud00079.cnf"
"SAT测试备选算例/满足算例/M/sud00082.cnf"
"SAT测试备选算例/不满足算例/u-homer16.shuffled-264.cnf"
"SAT测试备选算例/不满足算例/u-homer14.shuffled-300.cnf"
"SAT测试备选算例/不满足算例/u-x1_80.shuffled-238.cnf"
"SAT测试备选算例/不满足算例/mm-2x2-6-6-s.1.shuffled-as.sat03-1499-400.cnf"
)

# ---- 大型算例 ----
LARGE=(
"SAT测试备选算例/其它可供选择使用的算例/fla-600-1.cnf"
"SAT测试备选算例/其它可供选择使用的算例/fla-600-2.cnf"
"SAT测试备选算例/其它可供选择使用的算例/fla-600-3.cnf"
"SAT测试备选算例/其它可供选择使用的算例/fla-600-4.cnf"
"SAT测试备选算例/其它可供选择使用的算例/fla-600-5.cnf"
"SAT测试备选算例/其它可供选择使用的算例/fla-600-6.cnf"
"SAT测试备选算例/其它可供选择使用的算例/fla-600-7.cnf"
"SAT测试备选算例/其它可供选择使用的算例/fla-600-8.cnf"
"SAT测试备选算例/其它可供选择使用的算例/fla-600-9.cnf"
"SAT测试备选算例/满足算例/L/eh-dp04s04.shuffled-1075.cnf"
"SAT测试备选算例/不满足算例/u-dp04u03.shuffled-825.cnf"
)

# ---- 极难算例 (预期两版都超时, 验证 s -1 路径) ----
HARD=(
"SAT测试备选算例/满足算例/L/e-par32-3.shuffled-3176.cnf"
"SAT测试备选算例/满足算例/L/ec-iso-ukn009.shuffled-as.sat05-3632-1584.cnf"
"SAT测试备选算例/不满足算例/gt-030.shuffled-as.sat05-1295.cnf"
"SAT测试备选算例/不满足算例/eu-rand_net60-25-10.shuffled-3000.cnf"
"SAT测试备选算例/不满足算例/u-c6288.shuffled-5008.cnf"
)

TOTAL=$(( ${#SMALL[@]} + ${#MEDIUM[@]} + ${#LARGE[@]} + ${#HARD[@]} ))
DONE=0

echo "===== 小型算例 ====="
for c in "${SMALL[@]}"; do run_one "$c"; done

echo "===== 中型算例 ====="
for c in "${MEDIUM[@]}"; do run_one "$c"; done

echo "===== 大型算例 ====="
for c in "${LARGE[@]}"; do run_one "$c"; done

echo "===== 极难算例 ====="
for c in "${HARD[@]}"; do run_one "$c"; done

echo "===== 全部完成, 摘要见 $SUMMARY ====="
