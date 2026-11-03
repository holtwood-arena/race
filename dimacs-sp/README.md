# dimacs-sp — 第 9 届 DIMACS Implementation Challenge（Shortest Paths）

档案：http://www.diag.uniroma1.it/challenge9/ · DIMACS 主站：dimacs.rutgers.edu

Speed Demon Track D 的历史原型：真实美国公路网大图 + 点对最短路查询。
数据集与参赛 solver 均可公开下载，是检验"解析速度 + 内存布局 + 搜索循环"的经典题库。

## 数据集（challenge9 公开）

| 集 | 规模 | 用途 |
|----|------|------|
| USA-road-d.NY / BAY / COL 等 | 数十万~百万级节点 | 分区路网，快速迭代 |
| USA-road-d.USA（全美） | ~2400 万节点、~5800 万边 | 压轴规模 |

配套 `*.os` 查询文件提供 source-target 对；输出要求与评测口径见 challenge9 spec。

## 练习重点

- 边表快速解析（文本 → CSR），解析往往占总时大头
- CSR 内存布局与 cache 友好访问
- Dijkstra 的优先队列选型（binary / pairing / bucket），短查询主导时的开销
- 与 Track D 相同的取舍：预处理计入总时，重索引难以回本
