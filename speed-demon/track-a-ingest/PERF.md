# PERF — Speed Demon Track A: Ingest

任务：239 MB / 3,100,000 行 HTTP 日志 → 按 (hour, endpoint) 分组，
输出 count / p50 / p95 / p99 / errors / error_ppm，字节序排列，SHA-256 门禁。
Frozen spec：`spec.md`（官方仓库 `hackathon-raptors/speed-demon-2026` 拷贝）。

## 基线

- 日期 / commit / 命令 / 数据版本：
  - 2026-11-03，本文件所属 commit
  - `python3 reference/a_ingest.py --data data/full-seed1 --out out/full`
  - seed 1 `full`（本地 `generator/a_ingest.py` 生成，sha256 与 spec §5 官方值一致）
- 结果：T = 21.57 s · M = 213 MB（`/usr/bin/time -v`，单次）
- 参考实现：官方评分机 T_ref = 32.05 s（i5-13500，关 turbo，8GB 上限）；
  Apple M4 Pro 上同一参考实现 13.4 s / 237 MB。本机为 Ryzen 7 8845HS + WSL2，
  数字仅用于本地相对比较，**不代表官方评分机成绩**。

## 正确性校验（全部通过）

| 数据 | 输入 sha256 | 输出 sha256 | 结果 |
|---|---|---|---|
| tiny | `a99dd148…` | `c24fa507…` | ✅ 与 spec §5 一致 |
| sample | `7b22c678…` | `ca6311bb…` | ✅ 与 spec §5 一致 |
| full | `7ec80ba4…` | `495ca096…` | ✅ 与 spec §5 一致 |

## 参考实现瓶颈分析（来自 spec §7 + 代码阅读）

- 每行 `datetime.strptime`（RFC3339）— spec 称约占 80% 运行时间
- 文本模式逐行解码 + `split()` 产生大量临时 str
- `(hour_datetime, endpoint_str)` 元组作 dict key，每行两次哈希
- 每组一条不断增长的 Python list，最后全排序取 nearest-rank 分位数
- 单线程

优化方向（按预期收益排序）：

1. 手写字节级解析：固定 27 字节时间戳直接切片取小时；latency/status 用整数运算
2. endpoint 字典驻留：endpoint 全集仅 1,842 个，映射到整数 id，key 变 (hour_idx, ep_id)
3. 分位数：避免每组全排序的替代方案（两趟直方图 / 分桶 + 计数）待评估；
   每组保留数组但用更快的排序也在候选
4. 并行：输入按 chunk 切分给 ≤12 线程，最后合并分组
5. 语言切换：Python 上限有限，C++/Rust 重写是主赛道（本目录先走 Python 路线
   摸清算法，再评估换语言）

## 实验记录

| # | 日期 | commit | 改动 | T (s) | M (MB) | 结论 |
|---|------|--------|------|-------|--------|------|
| 0 | 2026-11-03 | 本 commit | 官方 Python 参考基线 | 21.57 | 213 | hash ✅，作为一切相对比较的锚点 |

## 失败 / 回退记录

- （暂无）

## 下一步

- [ ] Python 优化版 v1：字节解析 + endpoint→id + 整数 key，目标 < 5 s
- [ ] 评估分位数算法：全排序 vs 计数直方图（latency 是 uint32，范围大但每组 n 小，可能全排序反而快）
- [ ] C++ 版：mmap + 多线程，对齐官方容器契约（`python:3.13-slim` → 需换 allowlist 内的 `gcc:15/16` 或 `debian:13-slim`）
