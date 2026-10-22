# Week 5：让内存中的位置反复参加比赛

## 1. 本周解决什么问题

Week 4 每装满 M 个数就停止读取、排序并写出，因此一个初始 Run 最多 M 个数。但若刚输出最小值 3，接下来读到的是 8，8 完全可以留在当前 Run：它不会破坏顺序。把刚释放的位置用于新数据，就有机会在相同 M 个位置中生成更长 Run。

另一个问题是归并顺序。一个很长的 Run 如果很早参加合并，就会被反复读写。让短 Run 先合并，可以降低总搬运量。第三个问题是计算等待磁盘：读下一块、处理当前数据、写上一块可以交给三个线程，但必须规定谁拥有每块，以及错误如何结束整个任务。

## 2. 课件对照和工程选择

已读取本地课件第 39–43、59–60、72、78 页文字及相应嵌入图。

| 页码 | 知识点 | 实际实现 |
|---|---|---|
| 39–40 | 读入、计算、输出三线程；阶段交接与 I/O 重叠 | pipeline.cpp 的三个 std::thread |
| 41 | 置换选择；新值不小于刚输出值则留在当前段，否则冻结 | replacement_core 的 generation 标记 |
| 42–43 | 活动/预取输入、活动/写回输出、败者树工作区 | 两条有界块队列及计算线程私有树 |
| 59 | 首批准备、初始化工作区、最小值 x 被新值 y 替换 | 初始化 M 个槽，然后逐次 replace_winner |
| 60 | 后续阶段交换输入/输出角色，三阶段重叠 | 块通过 move 交接，生产/消费可独立推进 |
| 72 | thread/join、Loser Tree、置换选择 | ThreadJoinExample、LoserTree、ReplacementSelection |
| 78 | 置换选择、最优归并树 | BuildOptimalPlan，复用递归 ExecuteMergePlan |

**工程选择而非老师指定接口**：按 `(generation,value)` 比较；optional 表示耗尽；队列用 mutex/condition_variable；默认每条队列容量 Q=1；块携带 end_run 边界；异常保存为 exception_ptr；测试支持明确故障注入；以所有内部结点长度之和作为最优代价。

课件示意固定双缓冲，本程序采用“活动块＋有界待交接块”的动态移动所有权方案。还存在生产者私有块和编码 I/O 缓冲，**不能称整个程序恰好只有四块内存**。这是便于用标准库表达的基础流水线实现；没有忙等，也不要求三个线程每一轮都在全局屏障集合。它允许重叠，不保证一定更快，更不宣称实测达到完全重叠。

## 3. 一个完整置换选择例子

M=3，输入 `[4,1,3,2,6,0,5]`：

| 步骤 | 输出 | 新读入 | 处理 |
|---|---:|---:|---|
| 初始化 | — | 4,1,3 | 三个值都属于 generation 0 |
| 1 | 1 | 2 | 2≥1，仍属于当前 Run |
| 2 | 2 | 6 | 6≥2，仍属于当前 Run |
| 3 | 3 | 0 | 0<3，冻结到 generation 1 |
| 4 | 4 | 5 | 5≥4，仍属于 generation 0 |
| 5 | 5 | EOF | 对应槽标记耗尽 |
| 6 | 6 | EOF | 当前活跃元素全部输出 |
| 换段 | — | — | 冠军变成 generation 1，结束前段，开始下一段 |
| 7 | 0 | EOF | 结束全部输入 |

生成 `[1,2,3,4,5,6]` 和 `[0]`。0 虽然数值最小，却不能在 3 后直接输出，因此比较键先比 generation，再比 value。冻结不是丢弃，也不是复制到无限增长的备用数组：冻结值仍占用原来的一个槽。

算法中 `current` 是当前 Run 编号；新值小于刚输出值则赋 `current+1`，否则赋 `current`。当所有当前代元素都消失时，树自动把下一代冠军送到根，从而换段；不必 O(M) 扫描并重新标记所有冻结值。原始文件长度受流地址范围限制，generation 不会在本实现支持的文件规模内溢出。

升序输入可以整个文件成为一个 Run；严格降序一般每 M 个形成一段；随机独立输入的经典平均段长约 2M 是统计现象，不是保证。本实现测试确定性案例，不以“每个 Run 都达到 2M”作为错误断言。

## 4. 败者树到底存了什么

以四个值 `[4,1,3,2]` 比赛：4 对 1，内部结点记输家 4，1 晋级；3 对 2，记输家 3，2 晋级；1 对 2，记输家 2，冠军 1 单独保存。输出 1 后将同一个叶槽换成 5，只需要与它沿途遇过的输家重赛，不必重新比较 3 和 2 的子比赛。

`LoserTree<T,Less>` 位于 `include/external/loser_tree.hpp`：

- `values_`：每个参赛槽的 optional<T>；耗尽为 nullopt，不使用 INT_MAX 作为假无穷，因此 INT32_MAX 仍是合法数据。
- `base_`：不小于槽数量的最小二次幂，补齐树形；补齐叶用特殊下标表示不存在。
- `losers_`：每个内部结点保存输家下标，大小 base_；并非内部结点都存赢家的“胜者树”。
- `winner_`：全局冠军的槽下标，top 返回该槽的值。
- `less_`：可替换比较器；相等时用槽编号破平局，得到可复现的结果。
- `build(node)`：递归建树并向父结点返回赢家，一次 O(M)。
- `better(a,b)`：先处理不存在/耗尽，再按比较器和编号比较。
- `empty/top/winner_index`：检查是否还有有效元素，访问冠军值或编号；空树访问报错。
- `replace_winner(value)`：只更新原冠军槽，沿其父链与保存的输家重赛，O(log M)。该接口不支持任意叶直接更新；若要通用任意位置修改，需要另行设计维护方式。

这个类与文件格式无关，可用于多路流中“输出当前最小值，再由同一路补下一个值”的场景。

## 5. 串行版本与接口职责

先实现并实际测试串行，再接入流水线。两者共享 `detail::replacement_core`，防止并行版本悄悄改变算法。

`SelectionKey` 保存 generation/value；`SelectionLess` 按这两个字段比较。`replacement_core(memory,next,begin,put,end)` 的职责只有初始化槽、选冠军、换段、调用输出、读新值及替换。它不拥有文件，不创建线程，依赖四个回调适配外部 I/O。

`ReplacementSelection`：用 raw_count 读取 N，创建 InputBuffer、RunFileWriter，将四个回调绑定为 next/begin_run/put/end_run，最后显式 finish。最多预留 ceil(N/M) 个目录槽：在输入未耗尽时，一个 Run 结束前至少输出 M 个活跃元素；最后一段可以较短。空输入预留零槽，产生零个初始 Run。复用 Week 3 的目录容量上限 1,048,576，超限报错而非无限分配。

`ThreadJoinExample` 在子线程把 answer 设为 42，然后主线程 join 后读它。join 既等待线程退出，也让其写入对调用线程可见；若先读 answer 再 join，就没有这项同步保证。结果不用原子变量，是因为读操作确实发生在 join 后。

`BuildOptimalPlan` 调用 plan_leaves；priority_queue 用长度与编号比较；每次取最短两个，调用 Week 4 的 join_plans，再放回。其余实际归并仍由已验证的 ExecuteMergePlan 执行，不为“最优”另写一套归并内核。

## 6. 为什么最短两个先合并

复用模块仍由 `buffer.hpp/run_file.hpp/sort.hpp` 定义：`IOStats` 的八个字段分别统计 Data/Metadata 成功读写次数和字节；`RunEntry` 保存 id/count/offset/bytes；`InputBuffer` 的 offset/count/cursor 确定范围及顺序游标，block_start/valid/block 保存一块缓存；`OutputBuffer` 的 offset/used/block 保存写入位置和脏数据，失败后不可继续使用。`RunFileWriter` 的 begin/put/end/finish 管理活动段、非递减检查、尾块和最终有效头；`RunFileReader` 的 directory/input/scan/search/validate_sorted/display 定位和读取数据。

`RunRef` 保存 path/run/length；`MergePlanNode` 保存 type/id、source/outputPath、sequenceLength、estimatedCost、ioOperations、status 与左右孩子。`join_plans` 计算长度及累计代价；`PerformTwoWayMerge` 逐值择小，持有两个输入块及一个输出块；`ExecuteMergePlan` 后序递归，完成子结点后释放其缓冲并清理内部临时文件。Week 5 改变的是计划构造顺序，没有改变这些成员或操作语义。`pipeline.cpp::add_stats` 仅在所有线程 join 后汇总八个统计字段。

长度 `[1,2,3,100]`：

- 相邻配对先得到 3、103，再合并为 106，累计 3+103+106=212。
- 最短优先：1+2=3；3+3=6；6+100=106；累计 3+6+106=115。

每个叶的记录会被搬运多少次，等于它在树中的深度。代价可写成 Σ(叶长度×深度)，也等于所有内部结点长度之和。非负权重下，可以通过交换把两个最小权重放在最深的兄弟叶位置而不增加代价；将这对合并成一个权重更大的叶，得到规模更小的同类问题。这就是最短优先构造的依据。

最优针对这个**记录搬运模型**成立，且允许任意两个 Run 配对。精确块数还有 ceil 尾块、元数据和寻道影响，因此不保证最少实际磁盘操作或最短墙钟时间。空 Run 权重为零也能处理；没有初始 Run 返回 null；一个叶代价为零，但输出独立最终文件仍有一次复制成本。

## 7. 三线程中的所有权与同步

```text
读线程独占 raw 文件/InputBuffer
       ↓ move 输入块
incoming 有界队列（Q 个块）
       ↓ pop 后归计算线程独占
计算线程独占败者树、当前输入块和当前输出块
       ↓ move OutputChunk { values, end_run }
outgoing 有界队列（Q 个块）
       ↓ pop 后归写线程独占
写线程独占 RunFileWriter、OutputBuffer 和输出文件
```

`PipelineOptions` 保存 queue_blocks 和故障注入配置；正常运行 fail_stage=None。`PipelineMetrics` 返回两个队列的高水位，测试检查不超过容量。`OutputChunk` 是 pipeline.cpp 内部块，end_run=true 表示该块后结束当前 Run。恰好满块时会先发送非结束满块，再发送空的结束标记；写线程必须正确接受这个合法控制块。

`BoundedQueue<T>` 的每个成员：capacity_ 限制队列长度；queue_ 持有待移交对象；mutex_ 保护 queue、closed、cancelled、high_water；changed_ 用于唤醒；closed_ 是正常结束，不丢弃已排队数据；cancelled_ 是异常终止，不再消费数据；high_water_ 记录已达到的最大长度。

- push：等“有空间或关闭/取消”，成功后 move 进去并通知；关闭后推入报错。
- pop：等“有数据或关闭/取消”；正常关闭时先排空，之后返回 false；取消时抛异常。
- close：标记正常 EOF，notify_all。
- cancel：标记异常，notify_all，同时唤醒阻塞生产者和消费者。
- high_water：带锁读取；当前调用在 join 后，更容易理解生命周期。

所有等待都使用带谓词的 condition_variable::wait，正确应对虚假唤醒。队列传递值所有权，没有多个线程同时访问同一个 vector 的元素。Reader/Writer/IOStats 本身不是线程安全的，所以流水线分别使用 reader_stats 和 writer_stats，全部 join 后才汇总到调用者 stats。

## 8. 正常结束、异常与调用链

正常结束：读线程读完 N 条记录 → incoming.close → 计算线程排空输入后继续把树内剩余值全部输出 → 发最后一个 end_run → outgoing.close → 写线程排空输出并检查 count==N 且没有未结束 Run → finish → 主线程 join 三个线程。

异常结束：每个线程入口 catch(...) → fail 保存**第一个** exception_ptr（error_mutex 保护）→ cancel 两条队列 → 阻塞的 push/pop 被唤醒并抛取消异常 → 所有已创建线程 join → 汇总各线程成功 I/O → 删除本次失败输出 → 在主线程 rethrow 原始异常。线程创建到一半失败也走同一流程；不 detach，不让异常逃出线程入口触发 terminate。

原始输出路径必须不存在。单进程独占路径是前提，不支持其他程序同时创建/修改文件。失败清理使用 error_code，避免清理错误盖过原始错误；在文件系统拒绝清理时可能留下失败产物，不能称为事务提交。不会删除原始输入。

```text
串行：ReplacementSelection → replacement_core → LoserTree
                           ↘ InputBuffer / Writer 回调
并行：PipelineReplacementSelection → read / compute / write 三个线程
                                   compute → 同一个 replacement_core
后续：BuildOptimalPlan → join_plans → ExecuteMergePlan → PerformTwoWayMerge
```

测试注入位置：Read/Write 的 fail_after 是块序号，Compute 是输出记录序号，均从 0 开始。它只是测试入口，并非正常算法步骤。还测试真正的输出目录不存在错误，确保文件打开失败也能退出整条流水线。

## 9. 时间、空间、I/O 与限制

设 N 个记录、M 个树槽、B 个记录/块、Q 为队列块容量、R 为 Run 数、C 为归并树代价。

- 败者树初始化 O(M)，每次替换 O(log M)，空间 O(M)，不是每次取最小都线性扫描。
- 置换选择 CPU O(N log M+M)，串行记录工作空间 O(M+B)；Data 读 ceil(N/B) 次，写 Σceil(run_length/B)，字节均为 4N。每 Run 最后 flush，因此不能把写次数误写成始终 ceil(N/B)。
- 流水线算法操作量相同，另有块交接与锁成本；两条队列最多 2Q 块，线程私有块及编码缓冲还有常数个，总工作空间 O(M+(Q+1)B)。Q 固定时不随 N 线性增长。默认 Q=1 是选择，并非增加 Q 就一定更快。
- 最优计划构造 O(R log R)，元数据 O(R)。执行归并 Data 读写字节各 4C，具体块次数逐结点取整；记录缓冲 O(B)，递归栈 O(h)，最优树不一定平衡，h 最坏 O(R)。
- 复用 Reader 每次打开验证目录的元数据额外开销仍存在，可能 O(R²)；Data 和 Metadata 计数分开。
- 线程只优化置换选择的读/算/写阶段，最终归并仍是串行递归二路归并。不宣称实现并行归并或磁盘异步 API。
- std::thread 数量是三个工作线程，调用者作为第四个线程等待 join；不会为每块创建新线程。

## 10. 容易出错的边界与实际代码检查

INT_MIN/MAX 不能被哨兵占用；重复值的新输入用 `>=` 留在当前段；冻结值先按 generation 比较；EOF 后继续排空树；空输入不能发送虚假的空 Run；满块边界的空 end_run 标记不是错误；队列 close 与 cancel 不可混为一谈；队列满时任何阶段失败都要同时唤醒两条队列；统计量不能被两个线程并发 ++；join 前不要读线程写入的普通变量。

实际测试不是仅检查有序：串行每段有序与整体多重集对照；流水线文件与串行文件逐字节一致，Data I/O 次数也一致；最优代价与穷举所有两两配对顺序的 oracle 对比；10 万条最终结果用公式逐项验证。流水线测试设置 120 秒超时，防止死锁让测试无限挂起，但超时不是线程正确性的证明；本次未使用 ThreadSanitizer，也没有宣称做过竞态检测器验证。

## 11. 五道思考题

1. 在 M=3 的示例里，为什么读到 0 时不能直接输出？若把 generation 从比较键删除会怎样？
2. 败者树与“每个内部结点存最小值”的胜者树有什么区别？为什么接口只替换冠军？
3. 对 `[1,2,3,100]`，怎样用“叶长度×深度”再次算出 115？为什么该结果不必等于最少块 I/O？
4. 输入队列已满、输出线程此时打开文件失败，哪些线程可能正在等待？只取消输出队列是否足够？
5. 为什么单独 thread/join 示例的 answer 不需要 atomic，而让两条线程共享同一个 IOStats 并自增就不安全？
