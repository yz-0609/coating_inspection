# 编译、测试与烧录

所有以下命令在 Git 仓库根目录下的代码子目录 `coating_inspection_mcu/` 运行。本机已验证 CMake 3.28.1、Ninja 1.11.1、Arm GCC 12.2.0、原生 GCC / G++ 13.2.0、Python 3.11.7、OpenOCD 0.12.0。

## 预设

| 预设 | 输出目录 | 故障注入 | 用途 |
|---|---|---|---|
| debug | build | 关闭 | 默认开发构建 |
| release | build-release | 关闭 | 固件交付和默认 SWD 烧录 |
| debug-inject | build-inject | 显式开启 | 上板故障注入验证 |

三个预设保持 Cortex-M4F / Thumb / fpv4-sp-d16 / softfp，应用和 HAL 使用 -Os -g3，FreeRTOS 沿用 -O0。Release 不通过 NDEBUG 关闭断言；配置和输出目录隔离，普通 Debug 也不启用注入。

```powershell
python check_build_env.py
cmake --preset release
cmake --build --preset release --parallel
```

其他预设用相同命令替换名称。全量构建加入 `--clean-first`；清理目标使用 `cmake --build --preset release --target clean`。

## 原生主机测试

```powershell
cmake -S tests -B build-host -G Ninja -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++
cmake --build build-host --parallel
ctest --test-dir build-host --output-on-failure
python tools/offline_replay.py --emulator build-host/mcu_emulator.exe --output build-host/offline-replay.json
```

原生工程拒绝 Arm toolchain，不复用固件 CMake 缓存。测试使用与 MCU 相同的纯 C 模块，并对接未修改的上游 C++ 协议函数。回放采用虚拟毫秒时间，不提供 UART、DMA、GPIO 或真实持续运行证据。

## 全量检查与可分享交付

```powershell
python tools/validate_project.py
```

脚本运行三种固件全量构建、原生全量构建、CTest、回放、固件和来源校验，并保存 `build-validation/summary.json`、JUnit、日志和回放明细。依赖和注入检查失败、应用编译警告或测试失败都会中止交付。

单独校验：

```powershell
python tools/validate_firmware.py --build-dir build-release --expected-injection 0
python tools/validate_firmware.py --build-dir build-inject --expected-injection 1
```

生成 `delivery/firmware-1.0.0/`、固件 ZIP 和源代码 ZIP。固件目录有 ELF / BIN / HEX / MAP、文档、校验报告和 SHA256SUMS.json。源代码 ZIP 包含 CMake、依赖、源代码、主机测试、工具、Doc 和 VS Code 配置，排除生成目录和缓存。

## VS Code

在外层 `coating_inspection_mcu` 打开工作区。Ctrl+Shift+B 为普通开发构建；选择“编译交付版”“编译故障注入版”“主机测试”“离线回放”或“离线全量验证与交付”执行对应流程。所有烧录任务依赖交付版构建。

## SWD 烧录

实际板卡和引脚、电平确认后连接 SWDIO / SWCLK / GND / 目标电压参考及按需 NRST。当前候选配置尚未确认，不在离线验证脚本中执行烧录。

ST-Link：

```powershell
openocd -f interface/stlink.cfg -c "transport select hla_swd" -f target/stm32f4x.cfg -c "adapter speed 1000" -c "program {build-release/STM32F407.elf} verify reset exit"
```

CMSIS-DAP：

```powershell
openocd -f interface/cmsis-dap.cfg -c "transport select swd" -f target/stm32f4x.cfg -c "adapter speed 1000" -c "program {build-release/STM32F407.elf} verify reset exit"
```

OpenOCD 通过 PATH 查找。多调试器时配置实际 adapter serial。故障注入板上测试需把命令的路径显式改为 build-inject。IWDG 在启动调度前启用，调试暂停可能导致看门狗复位；正式故障验证必须记录调试器是否冻结 IWDG。

## 内存与历史记录

Flash 0x08000000 / 512 KiB；普通 RAM1 0x20000000 / 112 KiB；RAM2 0x2001C000 / 16 KiB 当前不用；初始主栈顶 0x2001C000。链接脚本给主栈预留 4096 B、C 堆 512 B。FreeRTOS 动态堆配置 15360 B，当前任务及队列均静态创建，未使用动态堆由链接器裁剪。

实际大小以当前构建的 firmware-validation.json 为准。原 build.log / validation.json / m0-*.log 为历史记录，不描述当前固件。M0 兼容检查入口 validate_m0.py 现在委托通用检查脚本。
