#!/usr/bin/env bash
# 模拟评分机约束跑容器：12 CPU、8GB、无网络、/data 只读、/tmp 1GB tmpfs
# 用法: ./run.sh <data_dir> <out_dir>
set -euo pipefail
cd "$(dirname "$0")/.."
DATA="$(realpath "${1:?data dir}")"
OUT="$(realpath "${2:?out dir}")"
mkdir -p "$OUT"
# WSL2: credsStore 指向不可用的 desktop.exe 时，用空 config 走匿名拉取
if grep -q 'desktop\.exe' ~/.docker/config.json 2>/dev/null \
   && ! command -v docker-credential-desktop.exe >/dev/null; then
  export DOCKER_CONFIG=/tmp/race-dcfg
  mkdir -p "$DOCKER_CONFIG" && [ -f "$DOCKER_CONFIG/config.json" ] || echo '{}' > "$DOCKER_CONFIG/config.json"
fi
docker build -q -t race-tracka -f docker/Dockerfile . >/dev/null
exec docker run --rm \
  --cpuset-cpus=0-11 --memory=8g --network=none \
  --tmpfs /tmp:size=1g \
  -v "$DATA:/data:ro" -v "$OUT:/out" \
  race-tracka
