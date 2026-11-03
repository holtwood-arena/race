# Speed Demon Track A — 练习用提交形态（allowlist: gcc:15 digest 固定）
# x86-64-v3 ≈ 评分机 i5-13500 的 AVX2 ISA；官方 build 在评分机上可用 -march=native
FROM gcc:15@sha256:eb5e64286ad47187b32bda75c6b6916166ecb41da56559b8b09e0001748d2cce
WORKDIR /app
COPY solutions/v4_evict.cpp .
RUN g++ -O3 -march=x86-64-v3 -pthread -o solve v4_evict.cpp
ENTRYPOINT ["/app/solve"]
