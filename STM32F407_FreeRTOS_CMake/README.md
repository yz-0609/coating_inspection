# 涂层质检 MCU 固件 1.0.0

沿用 STM32F407VET6、25 MHz HSE、168 MHz SYSCLK、HAL v1.8.2、FreeRTOS 10.3.1 和 GNU Arm CMake 工程。MCU 侧的软件功能已实现，默认协议为地址 1、115200、8E1、版本 0x0200；上位机生产代码无需修改。

当前接线仍为候选配置，`BOARD_WIRING_CONFIRMED=0`。本次仅完成离线构建、纯 C 测试、上位机 C++ 协议契约测试与虚拟回放；实际板卡、RS485 波形、BLE 仪器联调及真实 24 h 验收尚未执行。

## 构建与检查

在本目录执行：

```powershell
python tools/validate_project.py
```

该命令全量构建普通版、交付版和独立故障注入版，运行原生 C/C++ 的 CTest，校验固件格式、依赖哈希、SRAM 和注入隔离，再生成交付包。无需硬件或 pyserial。需要 Python 3.10+、CMake 3.21+、Ninja、Arm GCC，以及本机原生 GCC / G++。

单独构建交付版：

```powershell
cmake --preset release
cmake --build --preset release --parallel
```

VS Code 打开外层 `coating_inspection_mcu`，运行“MCU: 离线全量验证与交付”。原 SWD 烧录任务指向交付版 `build-release/STM32F407.elf`，核对真实接线后使用。故障注入版只由 `debug-inject` 预设显式启用。

## 当前实现

- 三个静态任务：HealthTask 100 ms / 优先级 5；CommTask 通知唤醒、最长等待 10 ms / 优先级 4；WorkstationTask 10 ms / 优先级 3。静态命令队列容量 8。
- 三路输入稳定 30 ms 去抖；ACTION / RESET 按下沿生成锁存事件；完整成功 ACK 空事件快照发送完毕后，经过 1200 ms 且按钮稳定释放才重新开放。
- 统一输出管理，PASS / FAIL 互斥、FAULT 覆盖，关闭电平在 GPIO 输出初始化前预装。
- USART2 RX 环形 DMA、TX DMA、TIM2 微秒时间和 TIM3 50 μs 活动监视；保留接收时间记录，在 USART TC 后释放 DE。
- 有界 Modbus RTU FC03 / 04 / 06 / 10，CRC、异常、22 字一致快照及完整命令入队。
- SYNC / RESET / START / RESULT / ACK_EVENT / ABORT，epoch / seq / token 校验、幂等、拒绝缓存、安全代次隔离旧排队命令。
- 变化心跳 1500 ms 保护、离位故障、任务进度检查、条件 IWDG 喂狗、复位原因、饱和诊断计数。
- USART1 有界非阻塞日志；交付版关闭输入和任务停滞注入。

## 目录

```text
Core/                       启动、时钟、IRQ、RTOS 入口
Bsp/Board/                  候选接线与致命故障输出
Bsp/Reset/                  RCC 复位原因采集
Bsp/UART/                   有界调试串口与 newlib 接口
Bsp/RS485/                  UART2 / DMA / TIM2 / TIM3
Bsp/Watchdog/               IWDG
Device/DigitalInput/        纯 C 去抖
Device/Output/              输出策略与 GPIO 管理
Module/Protocol/            协议字段、CRC、Modbus 服务
Module/Transport/           RTU 定界、时间记录环、DMA 游标
Module/Health/              任务进度期限
Module/Diagnostics/         复位编解码与诊断队列
Application/Workstation/     纯 C 工位状态机
Application/app_runtime.c   三任务、统一状态提交、队列和诊断
cmake/pure_modules.cmake    MCU / 原生测试共用的纯 C 构建清单
tests/                      自动测试、上游 C++ 契约、虚拟工位
tools/                      全量验证、回放、实机 CLI 与长稳记录
build-validation/           本次离线检查日志及 JSON / JUnit
build-release/              交付 ELF / BIN / HEX / MAP
delivery/                   固件包、哈希及源代码包
```

维护以 CMake、board_config.h 和手工源代码为准。`.ioc` 仅记录候选 GPIO、芯片和时钟，UART / DMA / IWDG / 原生任务由手工代码维护；不要直接再生成覆盖。

具体说明见 [软件交付与运行说明](../Doc/08_MCU软件交付与运行说明.md)、[实机验收清单](../Doc/09_MCU实机验收与已知限制.md)和 [构建烧录说明](CMAKE_BUILD.md)。
