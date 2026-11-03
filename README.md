# race — 性能工程竞赛实验室

> 集中研究与练习"同一任务、同一数据、同一机器，比参考实现更快"的赛事与公开题。
> 每个性能数字都绑定硬件、commit、命令与原始输出；本机数字只用于迭代，不代表评测环境。

**硬件基准**：AMD Ryzen 7 8845HS · 6 核 12 线程 · 16 MB L3 · WSL2。
正式赛事（如 Speed Demon）在主办方指定机器上计时，本仓数字仅为本地迭代记录。

## 题库索引

| 目录 | 题目 | 类型 | 状态 |
|------|------|------|------|
| [speed-demon/](speed-demon/) | Speed Demon 2026 四赛道真题（数据集与 harness 公开） | 解析 / 索引 / CSV / 图 | 🟡 进行中 |
| [1brc/](1brc/) | One Billion Row Challenge：13 GB、10 亿行聚合 | 解析 + 聚合 | ⚪ 未开始 |
| [ppc/](ppc/) | Aalto Programming Parallel Computers 练习题 | SIMD / 多线程 / cache | ⚪ 未开始 |
| [dimacs-sp/](dimacs-sp/) | 第 9 届 DIMACS 挑战赛：大图最短路 | 图算法 | ⚪ 未开始 |
| [ahc/](ahc/) | AtCoder Heuristic Contest 历届题 | 启发式优化 | ⚪ 未开始 |

## 成绩台账

每个数字注明：日期 · commit · 硬件 · 命令 · 原始输出位置。详情见各题目录下的 `PERF.md`。

| 题目 | 参考基线 | 当前最佳 | 倍率 | 峰值内存 | 日期 | 备注 |
|------|----------|----------|------|----------|------|------|
| speed-demon/track-a | 32.05 s（官方参考机） | 0.27 s（v4 C++ 12线程+驱逐，本机） | 80× | 156 MB | 2026-11-03 | hash ✅ 见 PERF.md |
| speed-demon/track-b | 56.35 s | — | — | — | — | |
| speed-demon/track-c | 14.44 s | — | — | — | — | hash 门槛最严 |
| speed-demon/track-d | 72.69 s | 1.55 s（v1 C++ CSR+并行，本机） | 45× | 254 MB | 2026-11-03 | hash ✅ 见 PERF.md |
| 1brc | 官方基线 ~4 min（评测机） | — | — | — | — | 需自备生成器产数据 |

## 方法

1. **先跑通参考实现**，确认输出 hash 一致，再谈优化——正确性是硬门槛。
2. **每步一记**：目录内 `PERF.md` 记录「改动 → 测量 → 结论」，失败的尝试也记。
3. **时间权重最大**：Speed Demon 的 K 系数中 wall time 占 0.50，优化优先级按公式而非直觉。
4. **并行预算固定**：目标机 6 物理核 12 线程，超订线程只亏不赚。

## 相关仓库

- 赛事提交物（赛期私有、赛后公开）：`speed-demon-2026`、`flagos-s2-*` 等，见 [repos-db](https://github.com/holtwood/repos-db) 竞赛索引
- 本仓只放练习与研究，不是任何比赛的提交格式
