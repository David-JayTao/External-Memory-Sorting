# 项目历史与 monorepo 迁移

## 审计和保护

迁移日期：2026-10-22。原工作区为 `Practice of Data Structures and Algorithms/`，原仓库分别是其 `week3/`、`week4/`、`week5/`。用户已确认其他任务停止修改 Week 5。

迁移前均为 main 分支、干净工作区、无 remote；真实提交数为 1、2、1。未发现未提交或未跟踪的源码修改。原始作者名称为 David-JayTao。原提交时间及说明未修改。

GitHub 指定仓库 `David-JayTao/External-Memory-Sorting` 审计为 Public，默认分支名 main，分支/标签列表为空，提交 API 返回空仓库状态。不存在需要合并的远程历史。发布前将再次确认，若远程变化则停止并重新评估。没有创建新 GitHub 仓库。

## 备份及恢复

根目录外的备份位置：`../external-memory-migration-20261022-021748-c230f5/backups/`。

每周包含 `weekN.bundle`、`worktree.zip`、`git-metadata.zip`、SHA-256 文件清单、原始状态和排除的可再生文件清单。bundle 已验证并实际克隆；两个 ZIP 已组合解压到新的恢复目录，验证 HEAD、工作区和引用。根 README、机器 VS Code 配置和完整课件另有本地 ZIP。未脱敏原始元数据只保留本地，不上传。

恢复时在新空目录中执行 `git clone <weekN.bundle> <destination>` 可恢复分支历史；完整恢复工具内部引用和工作区时，把 worktree.zip 与 git-metadata.zip 解压至同一个新空目录，再检查 git status/log。不要覆盖正在使用的工作区。build 和工具可重新生成，未放入工作区 ZIP，原产物保留或归档。

## 导入方法与映射

在备份 bundle 的临时克隆中使用 git-filter-repo 2.47.0；先排除课件提取目录，再添加 weekN/ 前缀。保留全部原始开发提交、作者名称、作者/提交时间、说明和顺序。为满足公开发布的隐私要求，将作者、提交者及标签作者邮箱改为该账号的 GitHub noreply 邮箱；未虚构作者或倒填时间。

Week 3 的 docs/ppt_evidence 从全部公开导入历史排除。工具内部 refs/codex/turn-diffs 的非开发树引用包含完整 PPT，只保留在本地备份，不导入或推送。过滤规则顺序的试验结果未通过检查，已弃用；最终导入结果逐提交通过路径及元数据检查。

| 原仓库 | 原提交 | 前缀迁移后的提交 |
|---|---|---|
| week3 | `a0beb57845f9f2bca50f6661331431991c0d1d93` | `67ce3a2af0223ad06ebbc9d2d0327162ae70b54a` |
| week4 | `3aaae799d82122d57991dbf8120a0d72e83480d2` | `51508699ea69dde1b2b077d22442d758719aef8a` |
| week4 | `543fc9bc8aebb26d7c776e4e0011e3579f663ef0` | `7327d2ad8f6f97dbe3f4cbb1b4ce61c47a6c7de8` |
| week5 | `c4ea84100a7ac85c04ec4e4a42823432395c5cfa` | `9cb6a768f7a1afc91396d16b8b8d47df633f66ca` |

完整映射、原始标签对象与新标签对象对应关系见 [history-mapping.json](migration/history-mapping.json) 及各 weekN-commit-map.txt。路径、课件过滤和邮箱脱敏导致 hash 改变。

旧标签保存为 legacy/weekN/<原标签>，不覆盖原始备份。旧 week5-v1.0 不作为新迁移的正式验收凭证。正式标签重新建立在各阶段实际测试通过的不同 monorepo 提交。legacy 标签仅保留本地并公布映射，不纳入本次正式标签推送。

## 演进时间线

- Week 3 原提交：2026-10-22 01:18:22 +08:00。
- Week 4 原实现：2026-10-22 01:34:21 +08:00；格式整理：01:45:19 +08:00。
- Week 5 原实现：2026-10-22 01:48:08 +08:00。
- 迁移顺序：Week 3 导入及验证 → unrelated histories 合入 Week 4 及验证 → 建立 feat/week5-pipeline → 导入 Week 5。
- 用户后续明确授权检查并完成 Week 5，再完成全部整合。因此 Week 5 在实现、测试与文档核验通过后，允许正常合并到 main 并建立正式标签；不再永远停留在“待验收”状态。

## 迁移后验证（持续追加）

这里只记录本次实际执行的迁移后验证。各周原有 docs/test_results.md 保留为历史记录，不能替代本次测试。

## 当前限制

尚需完成临时合并测试、最终路径工作区切换、源文件一致性与敏感扫描。验证通过前不发布。此次重点为仓库和文档，不进行算法重构。

### week3：临时 monorepo 重新构建与验证

执行 `powershell -NoProfile -ExecutionPolicy Bypass -File week3/scripts/build.ps1 -Configuration Debug -Demo`，以及同脚本 `-Configuration Release`。脚本实际运行 CMake 配置、编译和 CTest；原始日志保存在外部备份 audit 目录。

- Debug：退出码 0，1/1 测试通过；开始于 2026-10-22T12:12:49.310167+09:00，脚本总耗时 44.52 秒。
- Release：退出码 0，1/1 测试通过；开始于 2026-10-22T12:19:09.144106+09:00，脚本总耗时 37.56 秒。

### week4：临时 monorepo 重新构建与验证

执行 `powershell -NoProfile -ExecutionPolicy Bypass -File week4/scripts/build.ps1 -Configuration Debug -Demo`，以及同脚本 `-Configuration Release`。脚本实际运行 CMake 配置、编译和 CTest；原始日志保存在外部备份 audit 目录。

- Debug：退出码 0，2/2 测试通过；开始于 2026-10-22T12:23:25.326356+09:00，脚本总耗时 19.39 秒。
- Release：退出码 0，2/2 测试通过；开始于 2026-10-22T12:23:44.721569+09:00，脚本总耗时 24.83 秒。

### week5：临时 monorepo 重新构建与验证

执行 `powershell -NoProfile -ExecutionPolicy Bypass -File week5/scripts/build.ps1 -Configuration Debug -Demo`，以及同脚本 `-Configuration Release`。脚本实际运行 CMake 配置、编译和 CTest；原始日志保存在外部备份 audit 目录。

- Debug：退出码 0，4/4 测试通过；开始于 2026-10-22T12:25:09.618486+09:00，脚本总耗时 31.13 秒。
- Release：退出码 0，4/4 测试通过；开始于 2026-10-22T12:25:40.749561+09:00，脚本总耗时 35.41 秒。

### Week 5 完成度核验

依据用户后续“检查、补齐并完成全部整合”的授权，核查以下内容后通过技术验收：

| 要求 | 实现位置 | 本次验证 |
|---|---|---|
| std::thread/join | advanced.cpp、pipeline.cpp | join 可见性示例及三线程实际执行 |
| 可复用最小败者树 | loser_tree.hpp | 2,460 次冠军替换与独立最小值 oracle |
| 置换选择与活跃/冻结管理 | replacement_core.hpp、replacement.cpp | 15 组数据模式、有序性与多重集守恒 |
| 最优二路归并树 | advanced.cpp | 小规模穷举所有配对顺序；115 vs 212 代价对照 |
| 基础读/算/写流水线 | bounded_queue.hpp、pipeline.cpp | 21 组串行字节一致；有界队列与 9 次故障注入 |
| 异常和资源结束 | pipeline.cpp | 保存首个异常、取消两队列、join 全部线程及失败产物清理 |
| 完整外排序 | sort.cpp 与 pipeline.cpp | 100,000 条记录的公式 oracle |
| 学习文档 | week5/docs/week5_explanation.md | 页码、直观示例、结构职责、调用链、复杂度、边界和 5 道思考题 |

上述测试来自本次迁移后执行；未使用旧测试结果替代。本次无需修改算法或测试预期。开发分支保留真实导入及检查提交，随后以 --no-ff 合入 main，不 squash、不改写原始开发提交。
