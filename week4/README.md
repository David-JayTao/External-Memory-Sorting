# Week 4：基于归并计划树的外排序

统一仓库中的独立 C++17 / CMake 工程。复制并复用 Week 3 `a0beb57` 的文件/缓冲组件，保留其回归测试；不需要在构建时引用 week3 目录。

在 VS Code 打开本目录，Ctrl+Shift+B；或运行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build.ps1 -Configuration Debug -Demo
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build.ps1 -Configuration Release
```

工具查找本目录/父目录 `.tools/w64devkit/bin`、用户 Miniforge Scripts 及 PATH。其他平台可用标准 `cmake -S . -B build/manual`、`cmake --build build/manual`、`ctest --test-dir build/manual --output-on-failure`。

`week4_demo.exe <不存在的目录>` 生成原始文件、初始 Run 文件和最终文件，显示结果。目录必须不存在，避免覆盖。无第三方运行库依赖；工具和 build 不入 Git。

```text
include/external/{buffer,run_file,sort}.hpp
src/{buffer,run_file,sort}.cpp       文件层与外排序
src/week4_main.cpp                  演示
tests/week3_tests.cpp               文件层回归
tests/week4_tests.cpp               外排序测试
docs/week4_explanation.md           逐步讲解
docs/test_results.md                实测记录
scripts/build.ps1 / .vscode/        构建入口
```

核心入口：`RunGeneration`、`BuildBalancedPlan`、`PerformTwoWayMerge`、`ExecuteMergePlan`。原始输入是无头小端 int32 文件；最终结果是 Week 3 格式的单 Run 文件。空输入也生成一个空的最终 Run。

完整课件位于父目录 `外存数据管理.pptx`，不是构建依赖。原独立仓库标签已保存为 `legacy/week4/*`；新的正式 `week4-v1.0` 指向包含 Week 3/4 且通过迁移后测试的 monorepo 状态。所有开发在根仓库管理，详见 [项目历史](../docs/project-history.md)。`.clang-format` 提供四空格缩进及 100 列格式配置，格式工具不是编译依赖。
