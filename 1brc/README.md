# 1brc — One Billion Row Challenge

上游：https://github.com/gunnarmorling/1brc（2024-01 发起）

题目：解析约 13 GB、10 亿行温度测量记录（`站点名;温度`），输出每个站点的 min/mean/max，
按站点名排序。与 Speed Demon Track A/C 同源：自定义解析器、mmap、SIMD/SWAR、
手写哈希表、分块并行。

## 参考坐标

- 官方基线（Java，评测机 8 核）：~4 min
- 官方 leaderboard 头部：~1.5 s（评测机）；各语言社区复刻版各有榜单
- 本机为 Ryzen 7 8845HS / 6C12T，数字不可与官方榜单直接比较，只记录本地演进

## 数据

官方生成器生成 `measurements.txt`（~13 GB），`data/` 已 gitignore。
练习时可用缩减版（如 100M 行）先验证正确性，再上全量。

## 经典优化阶梯（按官方与社区 writeup 归纳）

1. 正确基线：逐行读、split、HashMap 聚合
2. 分块 + 并行：按行边界切分，各线程独立聚合后合并
3. mmap/大块读：减少 syscall 与拷贝
4. 自定义解析：手写站点名/温度解析，不走通用 tokenizer
5. SWAR/SIMD：`;` 与 `\n` 的定位向量化
6. 自定义哈希表：开放寻址、站点名就地比较，避免二次哈希

每步在 `PERF.md` 记录耗时与内存。
