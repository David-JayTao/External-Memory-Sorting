# External Memory Sorting

C++17 数据结构课程项目：块缓冲文件 I/O、外排序、归并计划、置换选择、败者树与读入/计算/写出流水线。

这是统一 Git 仓库，week3、week4、week5 是普通项目子目录。各周保持独立 CMake 工程与标准库实现，不依赖其他周的源码目录。

## 阶段状态

- Week 3：已完成，迁移后的 Debug/Release 测试和演示已通过；Header/Directory/Data、多 Run、Scan/Put/Search。
- Week 4：已完成，迁移后的 Debug/Release 测试和演示已通过；分批内排、流式二路归并、递归计划树。
- Week 5：实现、测试与文档检查已通过；迁移后 Debug/Release 全部通过，准备合入 main 并在最终路径复验。

## 环境与构建

需要 C++17 编译器、CMake >= 3.16；脚本使用 Ninja。Windows 本次验证环境为 GCC 16.2.0 / CMake 4.4.4。便携编译器可放在本地 .tools/w64devkit/bin；CMake/Ninja 可在 PATH 或用户 Miniforge Scripts。工具不上传。

在仓库根目录执行（相应阶段目录存在时）：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File week3/scripts/build.ps1 -Configuration Debug -Demo
powershell -NoProfile -ExecutionPolicy Bypass -File week4/scripts/build.ps1 -Configuration Debug -Demo
powershell -NoProfile -ExecutionPolicy Bypass -File week5/scripts/build.ps1 -Configuration Debug -Demo
```

将 Debug 换为 Release 可验证发布配置。VS Code 打开根目录，用 Tasks: Run Task 选择对应 Week。

工具已在 PATH 时，可使用标准 CMake 命令（以 Week 3 为例）：

```text
cmake -S week3 -B week3/build/manual -DCMAKE_BUILD_TYPE=Debug
cmake --build week3/build/manual --config Debug
ctest --test-dir week3/build/manual -C Debug --output-on-failure
```

Week 4/5 替换目录名即可。完整课件、便携工具、构建产物只保留本地。学习文档位于各周 docs/weekN_explanation.md。

## 分支与里程碑

main 按顺序接收稳定的 Week 3、4。Week 5 先导入 feat/week5-pipeline，完成实现、测试及文档检查后，再按用户最新授权合入 main，并保留开发分支便于追溯。

正式 annotated 标签 week3-v1.0、week4-v1.0、week5-v1.0 只在对应阶段验证后建立，指向不同阶段的真实稳定提交。原标签保存在本地 legacy/weekN/ 命名空间；映射公开，不冒充新的验收结果。

迁移记录、备份恢复与真实提交映射见 [项目历史](docs/project-history.md)。唯一远程为 https://github.com/David-JayTao/External-Memory-Sorting 。

## 项目目录

```text
.git/                       唯一 Git 元数据目录
README.md / .gitignore
.vscode/tasks.json          根目录构建任务
 docs/project-history.md    迁移、里程碑与实际验证记录
 docs/migration/            提交映射与机器可读验证摘要
week3/                      文件格式与块缓冲
week4/                      初始 Run 与二路归并计划
week5/                      败者树、置换选择与三线程流水线
```

各周都有 include、src、tests、docs、scripts、CMakeLists.txt 和 README。源代码没有跨周构建依赖。

- [Week 3 讲解](week3/docs/week3_explanation.md)
- [Week 4 讲解](week4/docs/week4_explanation.md)
- [Week 5 讲解](week5/docs/week5_explanation.md)
- [迁移后测试记录](docs/migration/validation.json)
