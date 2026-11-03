# ppc — Aalto Programming Parallel Computers

课程站：https://ppc.cs.aalto.fi （免费，自带评测机：提交代码后在对方硬件上计时排名）

与 Speed Demon 评测方式最像的练习场：不是看答案对不对就完事，而是真的计时。

## 章节与练习（按官方目录）

| 章 | 主题 | 练习代号 | 状态 |
|----|------|----------|------|
| 1 | 基础优化（指令级并行、流水线） | CP（correlation/Pearson） | ⚪ |
| 2 | 指令级并行与分支 | CP 进阶 | ⚪ |
| 3 | 多线程（OpenMP） | CP 多线程 / MF（median filter） | ⚪ |
| 4 | GPU（CUDA，需其环境） | SO（sorting）等 | ⚪ 可选 |
| 5 | 内存层次与 cache | IK（image kernel）等 | ⚪ |
| 6 | 向量指令 | 各题 SIMD 版 | ⚪ |

每题目录内放：参考实现、优化版本、`PERF.md`（本地耗时演进 + 官方 grader 分数）。

## 注意

- 官方 grader 的硬件与本机不同，本地数字只做相对改进参考
- 先保证正确（有容差要求，注意浮点精度约定），再逐档提速
