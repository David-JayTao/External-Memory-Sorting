# Week 5 实际验证记录

日期：2026-10-22（Asia/Shanghai）。Windows x64，GCC 16.2.0、CMake 4.4.4、Ninja、C++17、标准线程库。编译开启 `-Wall -Wextra -Wpedantic`，没有报告警告。

先编写串行版本并单独运行 `week5_serial`（5.67 秒，通过），再加入三线程流水线。之后 Debug 和 Release 全部构建、测试通过。代码格式整理并增强原始异常传播检查后，最终复验如下：

| 测试可执行文件对应 CTest 名称 | Debug | Release |
|---|---:|---:|
| week3_storage | 通过，1.54 s | 通过，1.55 s |
| week4_sort | 通过，6.86 s | 通过，6.64 s |
| week5_serial | 通过，2.46 s | 通过，2.94 s |
| week5_pipeline | 通过，2.93 s | 通过，2.98 s |
| 合计 | 4/4 通过 | 4/4 通过 |

复现命令（本仓库目录）：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build.ps1 -Configuration Debug -Demo
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build.ps1 -Configuration Release
```

## 实际检查内容

- Week 3 回归：8 组，1,011,295 次检查通过。
- Week 4 回归：24 个独立排序 oracle 案例、精确 I/O、输入异常、失败清理、100,000 条记录的公式校验。
- 败者树：槽数 0、1、2、3、7、16、31，2,460 次冠军输出/替换与独立线性扫描比较，包含相等值、耗尽、非二次幂槽数。
- 串行置换选择：M=1、3、16，各自测试空输入、升序、严格降序、全重复、随机与整数极值，共 15 个案例。每个 Run 有序、总多重集守恒、Data 字节守恒；之后执行最优树归并，与独立 std::sort 比较全部结果。
- 最优树：6 个权重案例，包括空树、单叶、零权重和不等长；用穷举所有两两配对顺序的独立递归 oracle 验证最低累计代价。`[1,2,3,100]` 的最优代价为 115，相邻配对为 212。
- 流水线：N=0、1、2、31、32、33、1003，B=1、7、32，共 21 个案例；文件逐字节等于串行输出，Data 读写次数和字节也一致。文件比较以固定 4096 字节块进行。
- 三阶段故障：Read/Compute/Write 各在事件序号 0、2、20 注入失败，共 9 次；检查主线程接到最初的注入异常，输出产物已清理。
- 真实错误：写线程打开不存在的父目录失败，整条流水线正常退出；已有输出不能覆盖；非法队列容量拒绝。
- 队列与重复运行：close 后排空，cancel 唤醒生产/消费路径，容量 Q=1/2 不超限；降序 10,003 条重复运行三次，均生成 589 个 Run，线程完成 join。
- 大文件：100,000 条数据通过流水线置换选择、最优树和递归归并；最终逐项等于 `index/1000`，未持有全量 oracle 数组。

测试输出原文片段：

```text
PASS serial: 2460 loser-tree replacements; 15 run-generation cases; optimal cost verified by exhaustive pair choices; thread/join
PASS pipeline: 21 byte-identical serial comparisons; 9 injected stage failures; real write-open failure; bounded queues; repeated joins; 100000 records through optimal merge
```

CTest 为流水线测试设置 120 秒上限，本次未触发超时。没有通过修改预期掩盖失败，也没有使用“理论上可运行”替代执行。

## 演示实际输出

```text
thread + join result=42
Replacement runs=16; balanced cost=4012; optimal cost=4012
Queue high water: input=1 output=1
Final count=1003 sorted=1
```

这组样本生成 16 个长度接近的 Run，因此两种计划代价相同；“最优”不意味着对每份输入都严格优于平衡树。不等长的代价差异由上述 115 vs 212 独立测试展示。

演示文件保存在 `build/Debug/demo-405f900d0a90474ea61ea3437bdce894/`，包括原始输入、串行/并行 Run 文件和最终结果。源码格式整理没有改变该演示的算法。

## 范围与限制

没有测试 MSVC/Linux，也没有运行 ThreadSanitizer；本报告不声称穷尽所有调度或证明没有竞态。实际采用线程私有文件/统计、带锁队列、异常取消及 join 生命周期，并用一致性和故障测试验证。未宣称线程版更快；计数是应用层 I/O，不是物理磁盘访问。

Git `main` / `week5-v1.0` 保存本周完整源码、测试、讲解与配置；构建产物、临时文件、工具不入库，仅本地提交。
