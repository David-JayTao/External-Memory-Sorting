# Week 3 实际测试记录

测试日期：2026-10-21（Asia/Shanghai）。以下结果来自本工作区真实编译、执行，不是推测结果。未修改测试预期来匹配输出。

## 环境

- Windows x64，PowerShell。
- C++ 编译器：便携 w64devkit 2.10.1，`g++.exe (GCC) 16.2.0`。
- CMake 4.4.4；Ninja 输出版本 `1.13.2.git.kitware.jobserver-pipe-1`。
- C++17，标准库，编译启用 `-Wall -Wextra -Wpedantic`；Debug / Release 均未出现编译警告。
- 原工作区只有 `外存数据管理.pptx`，不属于 Git 仓库；未发现已有课程代码或 AGENTS.md。
- CMake/Ninja 安装在现有 Miniforge Python 环境；编译器从 `https://github.com/skeeto/w64devkit/releases/tag/v2.10.1` 下载并解压到工作区 `.tools/w64devkit`。系统 PATH 未修改。

## 构建与 CTest

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build.ps1 -Configuration Debug -Demo
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build.ps1 -Configuration Release
```

| 配置 | 构建 | CTest | 本次测试用例耗时 |
|---|---|---|---:|
| Debug | 成功，7 个编译/链接步骤 | 1/1 passed | 1.48 秒 |
| Release | 成功，7 个编译/链接步骤 | 1/1 passed | 0.78 秒 |

CTest 注册一个测试可执行文件，该文件内部包含 8 个检查组，不能将 `1/1` 理解成只检查了一个边界。耗时会随机器负载与文件系统缓存变化，不用于性能承诺。

另外直接运行 `build/Debug/week3_tests.exe`，输出如下：

```text
PASS format, unequal runs, raw bytes and metadata I/O
PASS Scan ranges, empty runs and display
PASS InputBuffer cache, EOF and Search lower_bound
PASS OutputBuffer flush and failed/short I/O
PASS writer protocol, order rejection, limits and unfinished files
PASS corrupt header/directory, truncation and unsorted data
PASS deterministic randomized oracle across 5 buffer capacities
Large streaming case: 1000003 values, buffer=127, reads=7875, bytes=4000012
PASS million-record streaming data with bounded buffers
ALL PASSED: 8 groups, 1011295 assertions
```

检查并非 C assert 宏，因此 Release 不会移除这些检查。

## 覆盖范围与独立预期

1. **格式**：直接检查原始小端字节、magic、版本、头字段、目录起点、各 Run 偏移、负数编码、最终长度；不只依赖读写器互相验证。
2. **Scan**：完整、跨块区间、空 Run、EOF 空区间、非法 Run、越界范围、超大 count 溢出输入、显示文本。
3. **InputBuffer / Search**：同块复用、尾块、稳定 EOF、不存在值、首尾值、重复值首位置、空 Run、缓存命中下精确读次数。
4. **OutputBuffer**：满块自动写、部分块显式写、空 flush、重复 flush、短读、失败输出流、零容量。
5. **Writer 状态**：未 begin 的 Put、重复 begin、未 end 的 finish、目录已满、降序值拒绝且不污染数据、完成后操作、已有路径保护、INT32_MIN/MAX、零 Run 文件、未完成文件、非法目录容量、缺失文件。
6. **损坏文件**：14 种 Header/Directory 字段损坏，5 种文件长度异常，结构正常但 Data 无序时显式验证失败。
7. **确定性随机对照**：固定种子 123456，容量 1、2、3、16、100；每种 20 个不等长 Run；Scan 与独立小向量比较，Search 与 `std::lower_bound` 比较；精确检查读写次数和字节数。
8. **百万记录流式测试**：n=1,000,003，B=127；顺序生成和扫描，不保留完整数据向量。逐值顺序、数量、算术和都检查；读写次数预期均为 `ceil(n/B)=7,875`，字节数预期 `4*n=4,000,012`。缓冲编码数组为 508 字节；另有固定大小状态与标准流内部缓冲，不随 N 线性增长。

测试在自己新建的系统临时目录中创建文件，结束后清理；不删除用户原文件。小规模随机 oracle 的内存排序只属于测试数据准备，不用于外存实现。

## 实际演示输出

```text
Header: runs=3 capacity=5 data=224 end=256
Run 0 count=5 offset=224 bytes=20: -5 1 1 8 20
Run 1 count=0 offset=244 bytes=0:
Run 2 count=3 offset=244 bytes=12: 0 3 9
Scan Run 0 [1,4): 1 1 8
Search Run 0 key=1: 1
Search Run 0 key=7: not found
Data writes=3 bytes=32
Data reads=8 bytes=84
Metadata writes=5 reads=13
```

这次保留的演示文件是 `build/Debug/demo-cc4e302b42634d458e18676da1a22ad4.runs`。脚本下次运行使用新的唯一文件名。

演示读统计是显示、区间 Scan、两次 Search 的累计值；完整扫描三条 Run 本身只需 3 次 Data read，不能把总数 8 误当作单次扫描成本。所有统计都是应用层成功传输次数；没有声称测量物理磁盘读写。

## 已知范围

未测试 MSVC 或其他操作系统，因此只报告本机 GCC 的实际结果。项目为标准 C++17，CMake 另外配置了 MSVC 的 `/W4 /utf-8`，便于迁移，但不将其称为已验证。

未实现崩溃恢复、断电事务、数据校验码、并发写入、已封存 Run 的中间插入。二分 Search 以 Run 有序为前提，未知来源数据可先显式 validate_sorted。Week 4 / Week 5 未开始。

## 2026-10-22：Git 整理与目录迁移复验

工程迁移至独立仓库 `week3/`，公共编译器保留在父目录 `.tools/`，构建脚本已适配。完整 PPT 保留在父目录；课件节选随本仓库管理。上文 2026-10-21 的原始构建结果与演示文件保存在 `../artifacts/week3-initial-build/`，历史结果不作改写。

从父目录实际执行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File week3/scripts/build.ps1 -Configuration Debug -Demo
```

全新构建成功（7 个编译/链接步骤），CTest 1/1 通过，用例耗时 2.63 秒。演示输出与上文一致，生成 `build/Debug/demo-333d42c7c3504d329319f15a95cca6cf.runs`。本次只改目录、构建路径及版本管理配置，未修改算法或测试预期；未额外重复 Release 测试。

Git 使用 `main` 分支，首次提交包含现有 Week 3 完整实现，`week3-v1.0` 标签标记该基线。构建文件、`.runs` 数据、便携工具均已验证被忽略。没有配置远程或推送，也没有开始 Week 4 / Week 5。
