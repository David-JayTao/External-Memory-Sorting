# 第 11 章 外存数据管理 · Week 3

本项目仅完成 Week 3。C++17、CMake、标准库；真实二进制文件、定长块缓冲、不等长有序 Run。尚未实现 Week 4 / Week 5。

先读 [学习讲解](docs/week3_explanation.md)，再读 `include/external` 的接口和 `src` 的实现。

## Windows / VS Code

当前工作区提供了公共便携 GCC：`../.tools/w64devkit/bin`；CMake/Ninja 在用户的 `miniforge3/Scripts` 下。脚本优先查找本仓库 `.tools/`，再查找父目录公共工具，也支持 PATH 中的工具。换电脑时需准备 C++17 编译器、CMake、Ninja，或使用 Visual Studio 的 CMake 生成器。

在本仓库根目录 `week3/` 的 PowerShell 执行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build.ps1 -Configuration Debug -Demo
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build.ps1 -Configuration Release
```

也可以在 VS Code 按 `Ctrl+Shift+B`，执行已配置的构建、测试及演示任务。脚本只临时调整自身进程 PATH，恢复原值，不修改系统设置。每次演示创建一个新文件，防止覆盖已有数据。

工具已在 PATH 时，跨平台标准命令：

```text
cmake -S . -B build/manual -DCMAKE_BUILD_TYPE=Debug
cmake --build build/manual --config Debug
ctest --test-dir build/manual -C Debug --output-on-failure
```

单独运行演示：`build/Debug/week3_demo.exe new-example.runs`。路径必须不存在。演示生成三个 Run，打印 Header/Directory/数据、Scan、Search 与 I/O 统计。可重复使用 Reader 类打开已有文件；演示程序本身是固定教学流程，并非交互式数据库命令行。

## 目录

```text
CMakeLists.txt
README.md
.vscode/                 VS Code 构建任务
include/external/
  buffer.hpp             编码辅助、I/O 统计、输入/输出缓冲区
  run_file.hpp           目录项、文件写入器、文件读取器
src/
  buffer.cpp             字节编码、块读写、单块缓存
  run_file.cpp           文件结构校验、Put、Scan、Search
  main.cpp               完整演示
tests/week3_tests.cpp    边界、损坏文件、独立对照、百万记录测试
scripts/build.ps1        构建并执行 CTest，可选演示
docs/
  week3_explanation.md   算法与实际代码讲解，含 5 道思考题
  test_results.md        实测记录
  ppt_evidence/          指定页文字与原始嵌入图，供核对
../外存数据管理.pptx     公共原始课件（未修改，不在本仓库中）
```

`build/` 是生成结果；`.tools/` 是便携构建工具，不应提交到版本库。源码执行时没有第三方库依赖。

## Git 管理

本目录是独立 Git 仓库，使用 `main` 分支；`week3-v1.0` 标记可运行基线。源码、测试、文档、课件节选与 VS Code 任务纳入版本管理，工具和生成文件不入库。完整 PPT 位于父目录，独立检出仓库不包含完整课件，构建不依赖它。

VS Code 请打开本目录 `week3/`，再使用 Git 面板或 `Ctrl+Shift+B`。当前仅本地版本管理，未配置远程或推送。

```powershell
git status
git log --oneline --decorate
git diff
```

2026-10-22 整理前的构建结果移至 `../artifacts/week3-initial-build/`，旧 CMake 缓存仅作历史记录。构建脚本在本仓库 `build/` 下重新生成。
