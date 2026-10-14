# 上下位机通信协议

## 传输参数

RS485 / Modbus RTU，上位机为主站，MCU 为地址 `1` 的从站。串口为 `115200 / 8E1`，协议版本为 `0x0200`。寄存器内高字节在前，32 位字段高字位于低地址；CRC16 初值 `0xFFFF`，多项式 `0xA001`，低字节先发送。

RTU 使用 `t1.5 = 750 μs`、`t3.5 = 1750 μs`。MCU 在 USART TC 后释放 DE。CRC 或长度错误帧丢弃；不处理其他从站地址及广播业务写入。

## FC04 状态快照

上位机一次读取 `0x0000` 起的 22 个寄存器，MCU 返回一致快照。

| 地址 | 字段 | 定义 |
|---|---|---|
| `0x0000` | MCU_STATE | 0 BOOT、1 IDLE、2 READY、3 INSPECTING、4 COMPLETED、5 FAULT |
| `0x0001` | INPUT_FLAGS | bit0 到位、bit1 ACTION、bit2 RESET；均为逻辑有效状态 |
| `0x0002` | OUTPUT_FLAGS | bit0 PASS、bit1 FAIL、bit2 FAULT |
| `0x0003` | FAULT_CODE | 0 正常、1 心跳超时、2 检测离位、3 接收异常、4 任务异常、5 主动撤销、6 未同步 |
| `0x0004` | HEALTH_MASK | bit0 CommTask、bit1 WorkstationTask、bit2 HealthTask；完整健康为 `0x0007` |
| `0x0005` | MCU_HEARTBEAT | MCU 心跳计数 |
| `0x0006～0x0007` | UPTIME | 启动运行毫秒，32 位 |
| `0x0008～0x0009` | SESSION_TOKEN | 当前任务标识，32 位 |
| `0x000A～0x000B` | ACK_SEQUENCE | 最近处理命令的序号，32 位 |
| `0x000C` | ACK_STATUS | 最近命令的业务处理结果 |
| `0x000D～0x000E` | INPUT_EVENT_SEQUENCE | 锁存按钮事件序号，32 位 |
| `0x000F` | INPUT_EVENT_CODE | 0 无事件、1 ACTION、2 RESET |
| `0x0010` | RESET_REASON | 编码后的 MCU 复位原因 |
| `0x0011～0x0012` | COMM_ERRORS | 通信错误计数，32 位饱和累计 |
| `0x0013` | PROTOCOL_VERSION | `0x0200` |
| `0x0014～0x0015` | ACK_HOST_EPOCH | 最近处理命令所属主站周期，32 位 |

上位机以非零健康掩码作为快照健康条件；MCU 在 START / RESET 等执行点进一步检查完整任务健康和心跳期限。

## FC06 主站心跳

仅开放地址 `0x0100`。上位机默认每 200 ms 写入变化的 16 位值；业务健康时间戳超过 500 ms 时停止推进。MCU 仅在值变化时刷新有效时间，1500 ms 超时后锁存故障并撤销结果输出。

## FC10 完整命令

一次写入 `0x0110～0x0118` 的 9 个寄存器，禁止分段更新业务命令。

| 地址 | 内容 |
|---|---|
| `0x0110～0x0111` | HOST_EPOCH，32 位 |
| `0x0112～0x0113` | SESSION_TOKEN，32 位 |
| `0x0114～0x0115` | COMMAND_SEQUENCE，32 位 |
| `0x0116` | OPCODE，16 位 |
| `0x0117～0x0118` | ARGUMENT，32 位 |

| 操作码 | 命令 | 参数与用途 |
|---|---|---|
| 1 | SYNC_SAFE | 新 epoch 建立同步上下文；token 与参数为 0 |
| 2 | START | 携带非零任务 token，参数为 0，进入检测 |
| 3 | APPLY_RESULT | 当前任务 token；参数 1 PASS / 2 FAIL |
| 4 | ACK_INPUT_EVENT | 参数为锁存按钮事件序号 |
| 5 | RESET_FAULT | token 与参数为 0；健康恢复后解除故障 |
| 6 | ABORT | 携带任务 token，参数为 0，撤销任务并进入故障处理 |

FC03 支持心跳或最近处理命令寄存器回读。命令队列满时，FC10 返回 Modbus 异常 6；非法功能、地址和参数分别返回异常 1、2、3。

## 业务 ACK 与幂等

| ACK_STATUS | 含义 |
|---|---|
| 0 | 尚无处理结果 |
| 1 | 接受 |
| 2 | 工位状态不允许 |
| 3 | 任务 token 不匹配 |
| 4 | epoch、序号或命令身份不合法 |
| 5 | 心跳或内部任务健康不满足 |
| 6 | 操作码或参数不合法 |

FC10 正常响应表示命令成功入队。上位机继续轮询快照，检查 ACK epoch / 序号 / 状态，再核对操作对应的工位状态、token、输入和输出。默认确认期限为 1000 ms，最多重传 2 次，同一命令重传保持身份与内容。

MCU 缓存最近一次成功或拒绝结果。同身份、同内容重传复用缓存；同身份但内容变化返回身份错误。普通命令使用当前 epoch 和递增序号；新同步使用递增 epoch。故障或新同步递增安全代次，旧排队业务命令失效。

串口错误后上位机记录未知执行状态并重新同步。结果命令的历史 ACK 与当前输出条件一起检查，避免旧 ACK 被用于确认新输出。

## 按钮与输出

输入稳定 30 ms 后更新逻辑状态。ACTION / RESET 按下沿生成锁存事件，主站通过事件序号确认。MCU 清除事件后，等待对应的完整成功 ACK 快照发送结束、1200 ms 等待期及按钮稳定释放，再开放下一事件。

PASS / FAIL 互斥，FAULT 覆盖结果。检测中工件离位触发故障；就绪或完成后离位清除任务与结果。RESET 解除满足恢复条件的故障，不启动检测。

## 源码入口

- [上位机协议定义](../host/include/inspection/protocol.hpp)与[编解码 / 确认](../host/src/workstation/protocol.cpp)
- [MCU 协议定义](../mcu/Module/Protocol/protocol.h)、[寄存器服务](../mcu/Module/Protocol/modbus.c)与[工位状态机](../mcu/Application/Workstation/workstation.c)
- [两端契约测试](../mcu/tests/test_host_contract.cpp)
