# AGENTS.md — race 工作约定

本仓是性能工程竞赛练习 monorepo。维护者与读者先读本文件与 README.md。

## 结构约定

- 每个题库一个顶层目录：`speed-demon/`、`1brc/`、`ppc/`、`dimacs-sp/`、`ahc/`
- 每道题一个 `PERF.md`：记录「改动 → 测量 → 结论」，模板见 `docs/perf-template.md`；
  失败/回退的尝试同样记录并注明原因
- `tools/`：跨题共用脚本（bench.sh 等）；单题专用脚本放题目目录内
- `docs/`：方法论（playbook.md）、测量环境（environment.md）与模板
- `data/` 与大型二进制不入库（已 gitignore）；数据集一律从官方渠道获取

## 测量口径

- 每个数字绑定：日期 · commit · 硬件（`docs/environment.md`）· 命令 · 原始输出位置
- 默认跑 5 次，去最高/最低后取均值（对齐 Speed Demon 口径），统一用 `tools/bench.sh`
- 本机（Ryzen 7 8845HS, WSL2）数字只用于本地迭代；目标机参数见 `speed-demon/README.md`
- 不把本机数字外推为评测机性能；两处数字分开记录

## 正确性

- 输出与官方 expected hash 逐字节一致（sha256）才算通过；先正确后优化
- 非确定性输出本身就是正确性缺陷

## 变更记录

对外可见的结构/方法调整记入 `CHANGELOG.md`（Keep a Changelog）。
