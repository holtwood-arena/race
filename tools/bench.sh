#!/usr/bin/env bash
# bench.sh <runs> <cmd...> —— 跑 N 次，输出逐次耗时与去掉最高/最低后的均值（对齐 Speed Demon 口径）
# 用法: ./tools/bench.sh 5 ./run < data/input.txt > /dev/null
set -euo pipefail
runs="${1:?usage: bench.sh <runs> <cmd...>}"; shift
times=()
for i in $(seq 1 "$runs"); do
  start=$(date +%s%N)
  "$@" >/dev/null
  end=$(date +%s%N)
  ms=$(( (end - start) / 1000000 ))
  times+=("$ms")
  echo "run $i: ${ms} ms" >&2
done
sorted=$(printf '%s\n' "${times[@]}" | sort -n)
if [ "$runs" -ge 3 ]; then
  middle=$(echo "$sorted" | sed '1d;$d')
else
  middle="$sorted"
fi
mean=$(echo "$middle" | awk '{s+=$1; n++} END {printf "%.0f", s/n}')
echo "==> mean of middle $(echo "$middle" | wc -l): ${mean} ms" >&2
