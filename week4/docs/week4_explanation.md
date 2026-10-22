# Week 4：把“大排序”拆成可以放进内存的小任务

## 1. 问题与课件依据

假设文件有一百万个数，内存只允许同时排序一千个。不能把文件全读进 vector 再 sort。可先每一千个排好写回，形成一千个小 Run；随后两两合并，直到只剩一个 Run。归并时只需知道两个输入各自当前最小的剩余值，因此不需要把整个输入 Run 读入内存。

已检查实际 PPT 文本和第 34、36 页嵌入图。对应关系：第 33–35 页是初始归并段、两路输入和一路输出缓冲；第 36–38 页是计划树元信息和递归执行（先左、再右、再归并，清理临时文件）；第 71 页列出 RunGeneration、内排、缓冲、计划树；第 77 页要求基于归并树的外排序程序。

**工程选择**：原始输入为无头小端 int32；使用 std::sort 内排；初始 Run 共存于一个 Week 3 格式文件；每个内部结点生成独立单 Run 文件；本周计划按相邻结点逐层配对，奇数尾结点直接提升；不承诺加权代价最优。课件第 34 页提到异步预读，本周先实现可靠串行版本，线程流水线由 Week 5 完成。

## 2. 逐步实例

输入 `[7,1,5,2,9,0,3]`，M=3，B=2：

1. 前三项内排为 R0=`[1,5,7]`；第二批 R1=`[0,2,9]`；尾批 R2=`[3]`。
2. 第一层把 R0、R1 合并为 A=`[0,1,2,5,7,9]`，R2 不复制直接提升。
3. 第二层合并 A 和 R2，得到 `[0,1,2,3,5,7,9]`。

第一次归并只持有 R0 的 `[1,5]`、R1 的 `[0,2]` 和一个容量 2 的输出块。比较 1 与 0，输出 0；比较 1 与 2，输出 1，输出块满就写出。随后继续比较 5 与 2。某一路用完缓存才读下一块，某一路到 EOF 后排空另一边。相等时先取左边，不丢弃重复值。

初始 Run 长度为 3、3、1，内部结点长度为 6、7，累计归并代价为 13。每个内部结点都读和写自己长度的记录，总归并 Data 读/写字节各为 4×13=52；初始生成阶段另读写各 28 字节。

## 3. 为什么需要这些结构

- `RunRef` 是“文件路径、Run ID、长度”三元组。不等长 Run 不适合只靠文件名或固定长度推算。
- `MergePlanNode` 记录任务的依赖关系，不存记录值。先完成两个子任务，父任务才可执行；树让操作顺序、长度和代价都能检查。
- 两个 InputBuffer 让任一路独立补充；OutputBuffer 把零散结果合成块。三块容量都为 B，记录工作集不随文件大小增长。
- `vector<Value> chunk` 只用于 RunGeneration 的一个 M 元素批次，不累积整个文件。

## 4. 类、成员与关键函数

Week 3 的 `IOStats`、`InputBuffer`、`OutputBuffer`、`RunFileWriter`、`RunFileReader` 原样复用：前者分开计数 Data/Metadata；输入缓冲的 offset/count/cursor 指定 Run 和游标，block_start/valid/block 保存一块；输出缓冲的 used/offset 保存脏记录数与写入位置；Writer 管理 begin/put/end/finish；Reader 通过 directory 定位 Run。它们仍使用显式 finish，异常不被析构吞作成功。

新增接口位于 `include/external/sort.hpp`，实现位于 `src/sort.cpp`：

| 接口/结构 | 关键成员与职责 |
|---|---|
| `raw_count` | 文件长度必须是 4 的倍数，返回记录数 |
| `generate_raw` | 从 source(index) 逐个生成值，经 OutputBuffer 写文件，不构造全量数组 |
| `RunGeneration` | memory_records=M 限制 chunk，buffer=B 限制 I/O；预先用 ceil(N/M) 预留目录 |
| `RunRef` | path/run/length，定位一个外存序列，同时作为数据量校验依据 |
| `MergePlanNode::type/id` | Leaf 或 Internal；唯一编号用于内部临时文件名 |
| `source/outputPath` | 叶输入位置、已执行内部结点的输出位置；中间文件清理后路径仅作历史记录 |
| `sequenceLength` | 当前结点预期输出元素数 |
| `estimatedCost` | 子树内部结点长度之和，不是毫秒或精确块数 |
| `ioOperations` | 该内部结点本次归并的 Data read+write 次数，不重复累加子结点 |
| `status` | Pending → Running → Complete；异常变 Failed，计划只执行一次 |
| `leftChild/rightChild` | 拥有子计划的 shared_ptr；树只保存 O(R) 元数据 |
| `plan_leaves` | 逐条读取初始目录，建立叶结点列表 |
| `join_plans` | 创建父结点，带溢出检查地计算总长度与代价 |
| `BuildBalancedPlan` | 相邻两两配对，奇数尾结点提升，迭代至根 |
| `PerformTwoWayMerge` | verify_ref 核对输入长度；两个 next 游标择小写出；核对总输出数 |
| `ExecuteMergePlan` | 创建独占 scratch，处理零/单叶特例，递归执行并清理 |
| 内部 `execute` | 递归后序；子归并返回后其缓冲已释放，父归并才分配三块 |
| 内部 `copy_run` | 单叶仍写出独立最终文件，保持统一的单 Run 输出契约 |

工程中未直接照搬课件示例的全部字段：dataSize 可由 sequenceLength×4 得到，策略固定二路，因此未冗余保存 strategy。长度、代价是 uint64，运算溢出报错。

## 5. 调用链与数据流

```text
generate_raw → OutputBuffer → input.raw
RunGeneration → raw_count → InputBuffer → chunk(M) → sort
             → RunFileWriter → initial.runs
BuildBalancedPlan → plan_leaves → join_plans → root
ExecuteMergePlan → execute(left) → execute(right)
                 → PerformTwoWayMerge → 两个 InputBuffer → 一个 Writer
                 → 删除内部子输出 → 最终 sorted.runs
```

初始文件始终保留。递归完成子任务后不保留其读写缓冲，避免“每一层都挂着几块”的额外占用。scratch 必须是新目录，最终输出必须在其外面，避免清理 scratch 时误删最终结果。只删除内部产物，不删除叶文件。发生异常则清理自有 scratch 和本次失败输出；没有并发修改同一路径或断电事务保证。

## 6. 成本与内存边界

设总记录 N、初始段 R=ceil(N/M)、块 B、树高 h、累计内部权重 C。

- 初始生成 CPU O(N log M)，chunk O(M)，两个 I/O 块 O(B)；Data 读 ceil(N/B) 次，写 Σceil(run_length/B) 次，读写字节各 4N。
- 建平衡计划 O(R) 时间和空间，高 O(log R)。这是元数据内存，不可将整个算法空间误称为只有 O(B)。
- 单次归并 a+b 项，CPU O(a+b)，两个输入块和一个输出块共 12B 编码字节；读 ceil(a/B)+ceil(b/B)，写 ceil((a+b)/B)。尾块不会填充到整块大小。
- 所有归并 CPU O(C)，平衡计划通常 O(N log R)；Data 读写字节各 4C，精确块数逐结点求和。
- 递归栈 O(h)，记录缓冲 O(B)，计划 O(R)，不会把 N 条记录放进内存；初始生成和归并分阶段执行，峰值为 O(M+B+R)。
- 空输入无初始 Run，最终输出一个空 Run；单 Run 不需归并，但为生成独立最终文件多一次 O(N) 复制，这不计入树的 C，测试单独处理。

复用 Reader 的结构校验会在打开初始文件时扫描其目录；每个叶/归并重新打开可能带来 O(R²) 的元数据读取。它不加载全部记录，也不影响上述 Data 计数，但不是高性能目录缓存实现；IOStats 将其独立计入 Metadata，不能忽略这项成本。

## 7. 边界与实际测试思路

空文件、单元素、M 的前后边界、奇数 Run 数、负数与重复值、B=1、尾块、单边空 Run 均覆盖。原始字节不满 4、M=0、叶长度不匹配、父长度错误都应失败。输出已存在不能覆盖；失败后 scratch 必须消失，输入必须仍在。

小文件把完整结果与 std::sort 的独立 oracle 比较，检验的不只是“有序”，还包括没有丢失/重复记录。大测试生成 100,000 条 `99-i%100`，最终值应是 `index/1000`，用计数公式逐项验证，不保留大 oracle。测试还验证初始生成的块次数及归并的 4C 字节关系。

## 8. 五道思考题

1. 为什么 RunGeneration 的最后一批不必补满 M？其目录 count 应怎样写？
2. 两路归并比较相等时若同时前进两个输入，会丢掉什么信息？
3. 三个长度 3、3、1 的段按当前计划合并代价为 13；先合并 3 和 1 会是多少？
4. 为什么递归调用并不意味着每层都必须持有完整 Run 或缓冲区？
5. 若结果有序但少了某个重复值，“validate_sorted 通过”是否足以证明排序正确？还需要怎样的测试？
