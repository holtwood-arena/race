# Speed Demon 2026 — 四赛道练习

官方站：https://speeddemon.dev · 榜单/数据集：https://speeddemon.dev/board/#datasets

72 小时性能锦标赛（Hackathon Raptors 主办）。每个赛道给出任务、数据集与"正确但故意很慢"的
Python 参考实现；输出 SHA-256 必须与参考逐字节一致。本目录为个人练习用途，
正式参赛仓库单独建立（track-a/ ~ track-d/ 顶层目录 + Dockerfile 的提交格式）。

## 赛道与参考指标

| 赛道 | 任务 | 数据 | 参考时间 | 超时(10×) | 练习重点 |
|------|------|------|----------|-----------|----------|
| track-a-ingest | 310 万行日志聚合：按端点统计请求数、p50/p95/p99 延迟、按小时分桶错误率 | 239 MB | 32.05 s | 321 s | 行解析、时间戳解析、免排序分位数、分配器 |
| track-b-index | 250 MB 语料上 5 万条布尔/短语查询 | 256 MB | 56.35 s | 564 s | 倒排索引构建/查询的时间分配、posting list 压缩、位置信息 |
| track-c-join | 敌对格式 CSV（引号/换行/BOM/CRLF 混合）解析 + join 聚合 | 204 MB | 14.44 s | 144 s | **hash 门槛最严**：引用规则按 spec 不可妥协，边界用例优先 |
| track-d-traverse | 500 万边有向加权图 + 2000 个最短路查询 | 92 MB | 72.69 s | 727 s | 快速解析、CSR 布局、短查询为主的搜索循环 |

评分：K = (T_ref/T)^0.50 × (M_ref/M)^0.20 × (S_ref/S)^0.15 × (D_ref/D)^0.15，
speed score = 50·log10(K)，占总分 65%；评委（代码质量/技术/PERF.md）占 35%。

## 目标机环境（本地模拟时注意）

- Intel i5-13500：6 P 核 12 线程（AVX2，无 AVX-512），turbo 关闭，--cpuset-cpus=0-11
- 8 GB 内存上限、无网络、/data 只读、/tmp 为 1 GB tmpfs
- 无 io_uring（Docker 默认 seccomp 拦截），mmap 可用

## 获取数据

spec / 生成器 / 参考实现 / harness 均在官方仓库
[hackathon-raptors/speed-demon-2026](https://github.com/hackathon-raptors/speed-demon-2026)。
生成器确定性输出——`--seed 1 --scale {tiny,sample,full}` 本地生成与官方发布字节级一致
（sha256 见各 spec §5，已实测核对）。本目录 `data/` 与 `out/` 已 gitignore。

## 目录

| 子目录 | 说明 |
|--------|------|
| `track-a-ingest/` | 解析赛道练习：spec + 生成器 + 参考实现 + [PERF.md](track-a-ingest/PERF.md)（基线已建） |
| `track-b-index/` | 索引赛道练习 |
| `track-c-join/` | CSV join 练习 |
| `track-d-traverse/` | 最短路练习：spec + 生成器 + 参考实现 + [PERF.md](track-d-traverse/PERF.md)（v1 已建） |
