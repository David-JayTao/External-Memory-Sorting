> 历史快照：迁移前根 README。原文中的独立仓库说明已失效；构建方式以根 README 为准。保留迁移时发现的额外 `1` 行；仅规范行尾空白，原始字节另有本地备份。

# 数据结构课程实践工作区

每个 Week 使用独立 Git 仓库，父目录仅用于组织公共资料和本地工具。

```text
week3/                         Week 3 独立仓库（main 分支）
  .git/                        本周提交历史
  include/ src/ tests/          C++17 实现与测试
  docs/                        学习说明、测试记录、课件节选
  scripts/ .vscode/             构建脚本与 VS Code 任务
  build/                       新路径下构建结果，不入 Git
week4/                         Week 4 独立仓库：分批内排、二路归并、计划树
week5/                         Week 5 独立仓库：败者树、置换选择、最优树、三线程流水线
外存数据管理.pptx               公共原始课件，未修改
.tools/                        公共便携编译器，不入 Git
artifacts/week3-initial-build/  整理前的构建结果与演示数据存档
```

Week 3、Week 4、Week 5 均已实现，各自使用独立 Git 仓库。复用的前周组件已复制纳入对应仓库，每周可独立构建与提交，无跨仓库源码路径依赖。

VS Code 请“打开文件夹”打开对应的 **week3 / week4 / week5**，Git 面板和 `Ctrl+Shift+B` 都针对所选 Week。

在父目录执行：
1
```powershell
git -C week3 status
git -C week3 log --oneline --decorate
powershell -NoProfile -ExecutionPolicy Bypass -File week3/scripts/build.ps1 -Configuration Debug -Demo
powershell -NoProfile -ExecutionPolicy Bypass -File week4/scripts/build.ps1 -Configuration Debug -Demo
powershell -NoProfile -ExecutionPolicy Bypass -File week5/scripts/build.ps1 -Configuration Debug -Demo
```

讲解见 [Week 3 学习文档](week3/docs/week3_explanation.md)，运行说明见 [Week 3 README](week3/README.md)。

- [Week 4 学习文档](week4/docs/week4_explanation.md) · [运行说明](week4/README.md) · [测试结果](week4/docs/test_results.md)
- [Week 5 学习文档](week5/docs/week5_explanation.md) · [运行说明](week5/README.md) · [测试结果](week5/docs/test_results.md)

每周使用 `main` 分支。完成标签为 `week3-v1.0`、`week4-v1.0.1`、`week5-v1.0`。当前仅本地版本管理，没有配置远程或上传文件。源码、测试、文档及编辑器任务入库，编译产物、临时数据和工具不入库。

旧构建目录中的 CMake 缓存仍含旧绝对路径，仅作存档；请使用脚本在 `week3/build/` 重新构建。
