# 固件构建、测试与烧录

以下命令在仓库的 `mcu/` 目录运行。构建依赖 Python 3.10+、CMake 3.21+、Ninja 和 Arm GNU Toolchain；原生测试使用 GCC / G++，烧录使用 OpenOCD。

## 构建预设

| 预设 | 输出目录 | 故障注入 | 用途 |
|---|---|---|---|
| `debug` | `build/` | 关闭 | 开发构建 |
| `release` | `build-release/` | 关闭 | 交付与默认 SWD 烧录 |
| `debug-inject` | `build-inject/` | 开启 | 诊断输入与故障注入 |

Cortex-M4F / Thumb / fpv4-sp-d16 / softfp；应用与 HAL 使用 `-Os -g3`，FreeRTOS 使用 `-O0`。三个预设分别保存构建缓存，Release 保留断言。

```sh
python check_build_env.py
cmake --preset release
cmake --build --preset release --parallel
```

切换构建时替换预设名称。`--clean-first` 重新构建，`--target clean` 清理对应目录。

## 原生测试

```sh
cmake -S tests -B build-host -G Ninja -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++
cmake --build build-host --parallel
ctest --test-dir build-host --output-on-failure
```

测试工程独立配置，复用固件纯 C 模块，并直接编译 `../host/` 的协议实现。Python 路径可通过 `-DPython3_EXECUTABLE=<path>` 指定。

```sh
python tools/offline_replay.py --emulator build-host/mcu_emulator.exe --output build-host/replay.json
```

Linux 将模拟器路径改为 `build-host/mcu_emulator`。回放使用虚拟时间推进生产工位状态机。

## 全量验证与打包

```sh
python tools/validate_project.py
```

脚本执行三种固件构建、原生构建、CTest、回放、固件格式、第三方哈希、SRAM 和注入隔离检查。应用编译警告、测试失败或校验失败会中止打包。

| 路径 | 内容 |
|---|---|
| `build-validation/` | 构建日志、JUnit、汇总 JSON、回放与验证报告 |
| `build-release/` | ELF / BIN / HEX / MAP 与固件校验 JSON |
| `delivery/firmware-1.0.0/` | 固件、文档、校验记录和 SHA256SUMS.json |
| `delivery/firmware-1.0.0.zip` | 固件包 |
| `delivery/source-1.0.0.zip` | 上下位机源码、文档、开发任务与 CI 配置 |

单独校验固件：

```sh
python tools/validate_firmware.py --build-dir build-release --expected-injection 0
python tools/validate_firmware.py --build-dir build-inject --expected-injection 1
```

## SWD 烧录

连接 SWDIO、SWCLK、GND、目标电压参考，以及按需 NRST。板级引脚与输出有效电平按原理图配置。ST-Link：

```sh
openocd -f interface/stlink.cfg -c "transport select hla_swd" -f target/stm32f4x.cfg -c "adapter speed 1000" -c "program {build-release/STM32F407.elf} verify reset exit"
```

CMSIS-DAP：

```sh
openocd -f interface/cmsis-dap.cfg -c "transport select swd" -f target/stm32f4x.cfg -c "adapter speed 1000" -c "program {build-release/STM32F407.elf} verify reset exit"
```

OpenOCD 通过 PATH 查找。多调试器时设置 adapter serial。注入调试使用 `build-inject/STM32F407.elf`；IWDG 在启动调度前启用，调试暂停时结合调试器看门狗冻结设置操作。

## 内存与开发入口

Flash 起始 `0x08000000`，512 KiB；RAM1 起始 `0x20000000`，112 KiB；RAM2 起始 `0x2001C000`，16 KiB。主栈顶为 `0x2001C000`，主栈预留 4096 B，C 堆预留 512 B。DMA 缓冲与任务栈位于普通 SRAM。当前大小见构建生成的 `firmware-validation.json`。

VS Code 打开整个仓库，通过任务选择 MCU 构建、测试、回放、全量验证或烧录。上位机任务在 Linux 环境使用；任务目录分别指向 `host/` 与 `mcu/`。
