# 测试与验证指南

本仓库保留可重复执行的测试入口。测试输出包含运行范围、实际时长与结果；构建产物和报告由 `.gitignore` 排除。

## 上位机测试（Linux）

从仓库根目录：

```sh
cmake -S host -B host/build -DCMAKE_BUILD_TYPE=Debug
cmake --build host/build -j4
ctest --test-dir host/build --output-on-failure
```

| CTest 入口 | 内容 |
|---|---|
| `unit` | 规则、协议、数据库、状态机和异常边界 |
| `simulator_contract` | 独立工位模拟器的寄存器与通信契约 |
| `integration` | 完整任务、命令重试、断连、故障与事务恢复 |

内存与未定义行为检查使用独立目录：

```sh
cmake -S host -B host/build-sanitize -DCMAKE_BUILD_TYPE=Debug -DINSPECTION_SANITIZE=ON
cmake --build host/build-sanitize -j4
ctest --test-dir host/build-sanitize --output-on-failure
```

## MCU 原生测试（Windows / Linux）

```sh
cmake -S mcu/tests -B mcu/build-host -G Ninja -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++
cmake --build mcu/build-host --parallel
ctest --test-dir mcu/build-host --output-on-failure
```

| 入口 | 内容 |
|---|---|
| `reset_reason` | 复位原因编码与组合 |
| `modbus` | CRC、功能码、寄存器范围、异常和协议向量 |
| `workstation` | 状态机、命令身份、幂等与安全代次 |
| `inputs_health` | 去抖、事件确认、心跳和计时回绕 |
| `transport` | RTU 定界、接收记录环与 DMA 游标 |
| `pressure` | 100000 个随机 ADU、1000 次保护 / 结果交错、10000 次虚拟工位流程 |
| `upstream_host_contract` | 同仓库上位机 C++ 编解码 / 确认与 MCU 行为一致性 |
| `offline_replay` | 独立模拟进程中的命令、结果与恢复回放 |

测试编译生产纯 C 模块及当前 `host/src/workstation/protocol.cpp`，不维护单独的上位机代码副本。协议修改应运行两端测试。

## 固件构建与产物

在 `mcu/` 运行 `python tools/validate_project.py`，执行 debug、release、debug-inject 构建，以及原生测试、回放和产物校验。

固件校验包括 ELF / BIN / HEX 一致性、HEX 校验和、启动向量、ARM soft-float ABI、未解析符号、段权限、DMA 缓冲与任务栈位置、HAL 哈希以及故障注入字符串隔离。故障注入仅在 `debug-inject` 启用。

报告写入 `mcu/build-validation/`；生成固件和源码包位于 `mcu/delivery/`。详细命令见 [构建说明](../mcu/CMAKE_BUILD.md)。

## 演示、回放与运行监测

上位机 [`demo.py`](../host/tools/demo.py) 提供 PASS、FAIL 和中止演示，PTY 模式使用 Modbus 主站与独立工位模拟器。`soak.py` 记录运行时长、任务计数、资源采样与数据一致性。

MCU [`offline_replay.py`](../mcu/tools/offline_replay.py) 使用虚拟时间执行状态机回放；[`hardware_soak.py`](../mcu/tools/hardware_soak.py) 通过串口写入变化心跳并读取快照，输出采样 JSONL 和汇总 JSON。

## 持续集成

[CI 工作流](../.github/workflows/ci.yml) 在 Linux 环境构建上位机并运行 CTest 和演示，同时执行 MCU 固件全量校验、原生测试和打包。日志与报告作为 workflow artifacts 保存，每次 push 或 pull request 自动执行。
