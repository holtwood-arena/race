# Changelog

本文件遵循 [Keep a Changelog](https://keepachangelog.com/) 格式。

## 2026-11-03

- 初始化：题库目录（speed-demon / 1brc / ppc / dimacs-sp / ahc）、README 成绩台账、
  tools/bench.sh、docs/playbook.md、MIT LICENSE
- 补充 AGENTS.md 工作约定、docs/environment.md 测量环境、docs/perf-template.md 记录模板
- Speed Demon Track A 起步：拷入官方 spec/生成器/参考实现，本地生成三档数据集并核对
  sha256；参考实现基线 21.57 s / 213 MB（本机），输出 hash 与 spec §5 官方值一致；
  新建 track-a-ingest/PERF.md 记录瓶颈分析与优化方向
- Track A 优化三轮：v1 Python 字节解析 4.05s（5.3×）→ v2 C++ 单线程 mmap 0.39s（55×）
  → v3 C++ 12线程 0.27s（80×）；三档输出 hash 全部与官方一致，代码在 solutions/
- Track A v4：MADV_DONTNEED 逐段驱逐 mmap 文件页 + 整块缓冲输出，
  时间持平 0.27s，峰值内存 290→156MB（−45%）
- Track A 容器复测：docker/ 目录（gcc:15 allowlist 镜像 + run.sh 评分机约束），
  12CPU/8GB/无网络下输出 hash 全对
- Track D 起步：官方 spec/生成器/参考实现入库，v1 C++（mmap 并行解析 + CSR +
  查询级并行 Dijkstra）1.55s/254MB，三档输出 hash 与官方一致；PERF.md 记录
  幻影边、cursor 竞争两个真实 bug 及定位过程
- Track D v2 双向 Dijkstra：正确但实测更慢（2.33s vs 1.55s，内存翻倍），
  记为否定实验——查询 rank 分布偏小导致双堆开销收不回；v1 保持最佳
