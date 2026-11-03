# PERF — Speed Demon Track D: Traverse

任务：1,740,512 节点 / 5,000,000 有向边（uint32 权重，含 0）+
2,000 个 s→t 查询，输出最短路距离或 -1，逐行顺序 SHA-256 门禁。
预处理（解析+建索引）计入总时间。Frozen spec：`spec.md`。

## 基线

- 日期 / commit / 命令 / 数据版本：
  - 2026-11-03，本文件所属 commit
  - `python3 reference/d_traverse.py --data data/full-seed1 --out out/ref-full`
  - seed 1 `full`（本地生成器产出，sha256 与 spec §5 官方值一致）
- 结果：T = 70.50 s · M = 854 MB（`/usr/bin/time -v`，单次）
- 参考实现：官方评分机 T_ref = 72.69 s；Apple M4 Pro 上 43–48 s / 915 MB。
  本机为 Ryzen 7 8845HS + WSL2，本地数字仅用于相对比较。

## 正确性校验（全部通过）

| 数据 | graph.txt sha256 | distances.txt sha256 | 结果 |
|---|---|---|---|
| tiny | — | 与参考输出逐行 diff 一致 | ✅ |
| sample | `16351d40…` | `a4c76ed0…` | ✅ 与 spec §5 一致 |
| full | `fe6471f2…` | `a23dbdf0…` | ✅ 与 spec §5 一致 |

## 参考实现瓶颈分析（spec §7 + 实测）

- 每查询独立 heapq Dijkstra + dict dist，2000 次全重复
- Python 逐行 split + int() 解析 ~2 s
- 每查询 settle 中位 ~512 节点、均值 ~12,300；两个 no-in-target 查询各 settle
  整个主连通体（~3 s each on M4）
- 无跨查询复用、单线程

## 优化方向（按预期收益排序）

1. C++ CSR（off/head/wgt 三数组）：解析+构图 ~0.3-0.5 s
2. 查询级并行：2000 查询分 12 线程，每线程私有 dist/heap/touched
3. dist 数组用 touched-list 复位，避免每查询清零 1.7M 项
4. 早退：t settle 即停（参考也做）；不可达答案 -1 无法早退
5. 候选未做：双向 Dijkstra（对 rank ~131k 的少数长查询收益大）、
   A*/CH（CH 属 banned 索引类，不可自建库但手写是否算？——spec 禁的是
   "builds contraction hierarchies for you" 的库，自己写算法在规则内，
   但对 2000 查询 × 已很快的场景收益存疑，未做）

## 实验记录

| # | 日期 | commit | 改动 | T (s) | M (MB) | 结论 |
|---|------|--------|------|-------|--------|------|
| 0 | 2026-11-03 | 本 commit | 官方 Python 参考基线 | 70.50 | 854 | hash ✅，一切相对比较的锚点 |
| 1 | 2026-11-03 | 本 commit | v1 C++：mmap + 并行解析 + CSR + 查询级并行 Dijkstra | **1.55** | 254 | **45×**；解析 12 线程 + 查询 12 线程 |
| 2 | 2026-11-03 | 本 commit | v2 双向 Dijkstra：反向 CSR + 双堆 meet-in-middle | 2.33 | 526 | ❌ 更慢——见分析 |

### v2 双向搜索为何变慢（实测否定）

hash 全对但 1.55→2.33s（−50%）、内存 254→526MB。原因：

- **分布不匹配**：查询 rank 中位数仅 ~512、均值 ~12,300。双向的收益在
  大 rank（搜索域 ~rank² 缩到 ~rank），小 rank 查询反而白付双堆开销。
- **固定成本翻倍**：反向 CSR 多一趟填充（+~0.2s），每线程 dist×2/heap×2
  使查询期 cache 压力翻倍。
- **结论**：单向 + 早退在本分布已是最优形；双向只在 rank 普遍 ≥10⁵
  的场景才值得——本题不是。教训：先算查询分布再选算法。

### v1 实现要点

- 解析：12 线程按行界切块，两遍法（度数计数 → 前缀和 → 原子 cursor 填充）
- Dijkstra：手写二叉堆 + stale-skip + touched-list 复位 + t 早退
- 每线程状态：dist u64[n] (14MB) + heap + touched，12 线程 ~170MB
- 输出：手写 u64→str，单次 fwrite

### 已踩坑（详见失败记录）

- chunk 边界 `hi` 含上一条行尾的 `'\n'` → `num()` 返回 0 → 幻影边 (0,0,0)，
  11 个边界共 11 条，计入 deg[0] 使 off[n]=m+11 → CSR 越界。
  修复：行首检查 `isdigit`，非数字则跳过字节。
- 同 u 出现在多 chunk → 私有 cursor 冲突 → 改共享 atomic cursor。

## 失败 / 回退记录

- v1 首版堆损坏（ASan 定位 `head[e]` 越界）：根因为幻影边（上条），
  一度误判为 cursor 竞争——证据（BADE 的 u 全合法且是最大 u）才指向少计/多填。
- `munmap(graph)` 放在查询前正确（CSR 已拷贝数据）；endpoint-string_view
  类陷阱本题不存在（节点全是 id）。

## 下一步

- [x] ~~v2 双向 Dijkstra~~：实测更慢（2.33s），已记录分析，弃用
- [ ] v3 候选方向：解析+CSR 单趟化；4-ary 堆；查询按 rank 分桶后调度
  （长查询先跑防尾延迟）；touched 复位换 epoch 标记
- [ ] 容器复测：复用 track-a 的 docker/run.sh 模式
- [x] ~~CH/索引类~~：2000 查询下索引构建成本收不回，不做

## 复现命令

```bash
python3 generator/d_traverse.py --seed 1 --scale full --out data/full-seed1
python3 reference/d_traverse.py --data data/full-seed1 --out out/ref-full
g++ -O3 -march=native -pthread -o bin/v1 solutions/v1_csr_dijkstra.cpp
./bin/v1 --data data/full-seed1 --out out/v1-full
sha256sum out/v1-full/distances.txt   # 应等于 a23dbdf0…
```
