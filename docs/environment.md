# 测量环境

| 项 | 值 |
|----|-----|
| CPU | AMD Ryzen 7 8845HS（Zen 4，6C/12T，AVX2，无 AVX-512） |
| L3 | 16 MiB |
| 内存 | WSL2 动态分配（注意与 Windows 侧负载隔离） |
| OS | WSL2 · Linux 6.18 |

## 口径说明

- WSL2 下 boost 与调度不可控，数字方差大于裸机；**只与本地历史比较，不与任何榜单直接比较**
- Speed Demon 目标机：Intel i5-13500（6P 核 12T，AVX2 无 AVX-512，turbo 关，8 GB 上限）
- 换机/换环境时更新本表并在 PERF.md 里注明环境版本
