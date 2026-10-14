# STM32 工位控制器

STM32F407VET6 固件负责工件到位、按钮事件、Modbus 命令、结果输出与运行保护。使用 25 MHz HSE、168 MHz SYSCLK、STM32 HAL v1.8.2 和 FreeRTOS 10.3.1。项目整体介绍见 [仓库主页](../README.md)。

## 功能与结构

- 三个静态任务：CommTask 通信、WorkstationTask 工位状态、HealthTask 进度检查与条件喂狗。
- 到位与 ACTION / RESET 输入稳定 30 ms 去抖，按钮事件按序号锁存和确认。
- PASS / FAIL 互斥，FAULT 优先；输出关闭电平在 GPIO 初始化前写入。
- USART2 RX 环形 DMA、TX DMA、微秒时间戳与 RTU 定界，USART TC 后释放 DE。
- Modbus RTU FC03 / 04 / 06 / 10，22 字快照、9 字完整命令与业务 ACK。
- epoch / token / 序号校验、重复命令幂等、拒绝缓存及安全代次隔离。
- 变化心跳超时 1500 ms、离位故障、任务进度保护与 IWDG。
- USART1 非阻塞诊断日志、复位原因、通信计数与栈水位记录。

| 目录 | 内容 |
|---|---|
| `Core/` | 启动、时钟、IRQ 与 RTOS 入口 |
| `Bsp/` | 板级 GPIO、RS485、UART、复位与看门狗 |
| `Device/` | 输入去抖和输出策略 |
| `Module/` | 协议、传输、健康与诊断 |
| `Application/` | 工位状态机和任务运行时 |
| `tests/` | 原生测试、上位机协议契约和工位模拟 |
| `tools/` | 校验、打包、串口操作和监测 |

## 构建与交付

在本目录执行：

```sh
python check_build_env.py
cmake --preset release
cmake --build --preset release --parallel
```

全量验证与固件打包：

```sh
python tools/validate_project.py
```

工具需要 Python 3.10+、CMake 3.21+、Ninja、Arm GNU Toolchain 与原生 GCC / G++。普通 debug 与 release 关闭故障注入，`debug-inject` 显式开启诊断输入。构建入口以 CMake 和手工源代码为准，`.ioc` 作为芯片、GPIO 与时钟配置参考。

构建预设、原生测试与 SWD 命令见 [构建说明](CMAKE_BUILD.md)，接线见 [部署指南](../docs/DEPLOYMENT.md)，寄存器与命令见 [通信协议](../docs/PROTOCOL.md)。

## 串口诊断

```sh
python -m pip install -r tools/requirements.txt
python tools/mcu_cli.py --port COM3 snapshot
python tools/mcu_cli.py --port COM3 heartbeat 1
```

将 `COM3` 替换为实际串口，Linux 可使用 `/dev/serial/by-id/...`。持续心跳与快照监测：

```sh
python tools/hardware_soak.py --port COM3 --hours 24 --output build-validation/serial-monitor.json
```

同一 RS485 总线使用一个活动主站；诊断和持续监测期间由相应工具管理串口。USART1 调试串口为 115200 / 8N1，日志包含版本、时间、事件码和参数。

## 组件来源

保留 [HAL](Drivers/STM32F4xx_HAL_Driver/LICENSE.txt)、[CMSIS](Drivers/CMSIS/LICENSE.txt) 和 [FreeRTOS](Middlewares/Third_Party/FreeRTOS/Source/LICENSE) 许可证。HAL 文件来源与哈希记录见 [M0_VENDOR_MANIFEST.json](Drivers/STM32F4xx_HAL_Driver/M0_VENDOR_MANIFEST.json)。协议契约测试直接使用同仓库 `host/` 源码。
