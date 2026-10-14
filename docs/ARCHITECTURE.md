# 系统架构

## 系统边界

Linux 上位机负责测量、规则、检测任务与持久化；STM32 控制器负责工位输入输出、命令执行和运行保护。两端通过 Modbus RTU 交换状态与命令，BLE 测厚仪由上位机通过 BlueZ 接入。

## 上位机线程与对象

```text
仪器线程 ── 测量 / 连接事件 ──┐
工位线程 ── 快照 / 命令反馈 ──┼─→ 有界事件队列 → 业务线程 → SQLite
CLI 线程 ── 操作命令 ─────────┘                      └→ 工位命令队列
```

`app/main.cpp` 装配配置、队列、后端和工作线程。`InspectionWorker` 是活动任务、采集窗口和候选数据的状态所有者。SQLite 连接在业务线程中创建和释放，其他线程通过事件交互。

`IInstrument` 统一 BLE 与回放后端；`IWorkstation` 统一 Modbus 与内存工位；`IClock` 提供可替换的时间来源。配置选择后端，业务流程沿用相同接口。

| 流程阶段 | 作用 |
|---|---|
| `IDLE / READY` | 等待工件与开始条件 |
| `START_PENDING` | 任务已创建，等待 MCU 确认开始 |
| `WAIT_ARM` | 等待开启当前点采集窗口 |
| `WAIT_NEW_MEASUREMENT` | 接收窗口开启后、同连接代次的有效测量 |
| `WAIT_CONFIRM` | 展示候选编号，等待操作员确认 |
| `FINALIZING` | 从持久化点位复核规则并提交结果 |
| `COMPLETED` | 检测结果已保存，现场执行状态单独记录 |
| `ABORTED / FAULT` | 保存中止信息并执行恢复流程 |

数据库任务状态与流程阶段分别管理。候选展示和共享编号更新使用同一互斥锁；工位快照入队时绑定已展示编号，使按钮确认与操作员看到的数据一致。

## MCU 分层与任务

| 层 | 职责 |
|---|---|
| `Core/` | 启动向量、时钟、IRQ、HAL 与 RTOS 入口 |
| `Bsp/` | 板级 GPIO、RS485、调试 UART、复位与 IWDG |
| `Device/` | 输入去抖、输出门控与 GPIO 提交 |
| `Module/` | Modbus、RTU 定界、DMA 游标、健康与诊断 |
| `Application/` | 纯 C 工位状态机与 FreeRTOS 运行时 |

`CommTask` 处理接收记录、帧解析、快照和命令队列；`WorkstationTask` 每 10 ms 推进去抖与业务；`HealthTask` 每 100 ms 检查进度并决定是否喂狗。三个任务及容量为 8 的命令队列采用静态创建。

工位状态、输出和健康门控在短临界区内一起提交；编码、日志格式化和串口发送在临界区外完成。RX 使用 512 B 环形 DMA，TX 使用普通 DMA，发送后等待 USART TC 再释放 DE。

## 事务与运行恢复

确认点独立提交；最终结果与 `InspectionCompleted` Outbox 事件原子提交。`actuation_log` 独立保存命令执行状态，使质量判定与现场执行可分别追踪。

数据库异常、队列溢出或业务停滞会停止业务健康推进。工位根据变化心跳期限撤销结果输出；内部任务异常则停止喂 IWDG。重新同步创建新 epoch，隔离旧命令和事件。

上位机重启将未完成任务标为中止，并将未确认命令收尾为 UNKNOWN。SIGINT / SIGTERM 通过主线程协调保存中止、撤销命令和线程停止。

## 两端协议维护

协议定义分别位于 [`host/include/inspection/protocol.hpp`](../host/include/inspection/protocol.hpp) 与 [`mcu/Module/Protocol/protocol.h`](../mcu/Module/Protocol/protocol.h)。MCU 契约测试直接编译当前上位机的编码、解码和 `command_satisfied`，两端协议修改在同一提交中维护并验证。

云扩展使用 `ICloudPublisher`、稳定 `event_id` 和单次请求 `request_id`。默认适配器为本地运行模式，事件保存在 Outbox。
