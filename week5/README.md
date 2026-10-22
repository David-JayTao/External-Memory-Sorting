# Week 5：置换选择、最优归并与三线程流水线

统一仓库中的独立 C++17 / CMake 工程，复用 Week 4 `3aaae79` 的外排序组件，保留 Week 3/4 回归测试；无需在构建时引用其他 Week 的目录。

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build.ps1 -Configuration Debug -Demo
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build.ps1 -Configuration Release
```

VS Code 打开本目录后 Ctrl+Shift+B。工具查找规则同 Week 4，支持公共 `../.tools/w64devkit/bin`、用户 Miniforge Scripts 及 PATH。CMake 通过 `Threads::Threads` 配置线程库。其他平台可使用标准 CMake 构建及 CTest 命令，本次实际验证环境为 Windows GCC。

`week5_demo.exe <不存在的目录>` 实际执行 thread/join 示例、串行置换选择、流水线置换选择、最优计划构造及最终外排序。目录中保留 raw、两份初始 Run 文件和最终有序文件。

```text
include/external/
  buffer.hpp run_file.hpp sort.hpp  复用文件与归并组件
  loser_tree.hpp                    泛型最小败者树
  replacement_core.hpp              串行/并行共用算法状态机
  bounded_queue.hpp                 有界队列、关闭与取消
  advanced.hpp                      Week 5 对外接口
src/
  advanced.cpp                      thread/join、最优归并树
  replacement.cpp                   串行置换选择
  pipeline.cpp                      三线程流水线
  week5_main.cpp                    演示
tests/
  week3_tests.cpp week4_tests.cpp    回归测试
  week5_serial_tests.cpp             败者树、置换选择、最优树
  week5_pipeline_tests.cpp           一致性、限流、故障与退出
docs/week5_explanation.md            学习文档与 5 道思考题
docs/test_results.md                 实测记录
```

完整课件保留在父目录，不是构建依赖。生成文件和工具不入 Git，`main` / `week5-v1.0` 标记完成基线。只进行本地版本管理。
