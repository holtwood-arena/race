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
| 1 | 2026-11-03 | 本 commit | v1 字节解析：整块读入 + 整数键分组 | 4.05 | 413 | 5.3×；内存 +200MB 来自全量驻留 + int list |
| 2 | 2026-11-03 | 本 commit | v2 C++ 单线程：mmap + memchr + unordered_map | 0.39 | 259 | 55×；RSS 大头是 mmap 文件页本身 |
| 3 | 2026-11-03 | 本 commit | v3 C++ 12 线程：行界切块并行解析 + 合并 | 0.27 | 290 | 80×；仅比单线程快 1.44× |
| 4 | 2026-11-03 | 本 commit | v4 = v3 + MADV_DONTNEED 逐段驱逐文件页 + 整块缓冲输出 | **0.27** | **156** | 时间持平，内存 −45% |

### v4 驱逐策略说明

每线程消费 4MB 后，对已读区域做 `madvise(MADV_DONTNEED)`——mmap 的文件页
在缺页时计入 RSS，驱逐后峰值只剩各线程在飞窗口（~12×4-8MB）+ 数据结构
（latencies ~40MB）。零时间代价：约 60 次额外 madvise 调用。
踩坑：endpoint 是 `string_view` 指进文件映射，`munmap` 必须留到进程退出
（首版提前 munmap → segfault）。

### v3 并行加速比低的解释

0.39s → 0.27s 远低于 12 线程理论值。此规模下瓶颈已不是逐行解析：
- 239MB 的 mmap 缺页 + 内存带宽是共享地板
- 串行尾：合并分组 + 每组排序 + 127k 行 fprintf 输出不可分摊
- unordered_map 查找与 vector push_back 本身已接近缓存友好下限

结论：Track A 在"能并行解析"解决后进入收益递减区；下一步收益在
**内存**（mmap 页计入 RSS，换成分块 read 可把 290MB 压到 ~50MB，
K 的 M 项权重 0.20）与**输出路径**（bufio 替代逐行 fprintf）。

## 失败 / 回退记录

- v1 首跑 `IndexError`：hidx 是跨 4 天的绝对小时（实际取值 18–89），按 72 开数组越界。
  改 SPAN_DAYS*24=96 后通过——教训：输出排序的小时枚举范围 ≠ 数据集小时数。
- v1 排序键一度用 (hidx<<11)|epid 直接排：ep_id 是首见顺序而非字节序，
  会打乱同小时内 endpoint 次序。已修正为按小时桶内对 endpoint bytes 排序。

## 下一步

- [x] v4 内存优化：驱逐文件页达成 156MB（未达 <100MB：剩余大头是 latency 数组
  ~40MB + 在飞文件页 ~90MB；再压需更小步长或 chunked read，收益递减）
- [x] v4 输出优化：整块缓冲 + 手写 u32 格式化，一次 fwrite
- [ ] 容器对齐：按官方 allowlist 做 `gcc:15` Dockerfile，容器内复测
- [x] ~~分位数 histogram~~：n 均值 ~24，全排序已足够快，不追

## 复现命令

```bash
# 数据（确定性，sha256 见 spec §5）
python3 generator/a_ingest.py --seed 1 --scale full --out data/full-seed1
# 参考基线
python3 reference/a_ingest.py --data data/full-seed1 --out out/ref-full
# v1
python3 solutions/v1_bytes.py --data data/full-seed1 --out out/v1-full
# v2 / v3（产物在已忽略的 bin/）
g++ -O3 -march=native -o bin/v2_single solutions/v2_single.cpp
g++ -O3 -march=native -pthread -o bin/v3_parallel solutions/v3_parallel.cpp
./bin/v3_parallel --data data/full-seed1 --out out/v3-full
sha256sum out/*/summary.txt   # 全部应等于 495ca096…
```
