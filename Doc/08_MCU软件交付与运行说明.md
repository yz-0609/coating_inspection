# MCU 软件交付与运行说明

固件版本：1.0.0；协议版本：0x0200。当前可离线实现的软件功能、测试、构建和交付工具已完成。按用户最新要求继续开发，候选接线未作为已确认硬件，上板和真实仪器验收未执行。

## 交付和一键检查

工程：`coating_inspection_mcu`。在该目录执行 `python tools/validate_project.py`，完成三个固件变体全量构建、原生 C/C++ 测试、虚拟回放、依赖与固件校验、打包。VS Code 使用外层工作区，任务“MCU: 离线全量验证与交付”运行同一流程。

| 交付项 | 位置 |
|---|---|
| 交付固件 | build-release/STM32F407.elf / .bin / .hex / .map |
| 有显式注入入口的调试固件 | build-inject，不能替代交付版 |
| 普通开发固件 | build，默认也关闭注入 |
| 固件版本、哈希、内存与来源检查 | 各构建目录的 firmware-validation.json |
| 检查日志、JUnit、JSON 和回放明细 | build-validation |
| 可分享固件目录与 ZIP | delivery/firmware-1.0.0，firmware-1.0.0.zip |
| 可重新构建的完整源代码 ZIP | delivery/source-1.0.0.zip |
| 原生测试 / 工位回放程序 | tests / build-host/mcu_emulator.exe |
| 主站 CLI / 真实串口长稳记录 | tools/mcu_cli.py / tools/hardware_soak.py |

默认烧录任务依赖交付构建，指向 build-release。HAL 新增文件固定官方 v1.8.2，有 SHA-256 来源记录；HAL/CMSIS/FreeRTOS 许可证和版权保留。未改动上位机生产仓库。测试中保留上游两个原文件，固定提交 `aab161d065bab147a8894b28832d413973821423`，出处及哈希见 tests/vendor/host_protocol/PROVENANCE.json。

## 接线和外设配置

| 功能 | 资源 | 配置 / 默认态 |
|---|---|---|
| 芯片 / 系统时钟 | STM32F407VET6 / HSE25 MHz | PLL M25 N336 P2 Q4，SYSCLK168 MHz |
| APB1 / APB2 | 42 / 84 MHz | 对应定时器时钟 84 / 168 MHz |
| 到位 | PA0 | 高有效，内部下拉 |
| ACTION / RESET | PA1 / PA4 | 低有效，内部上拉，周期采样 |
| PASS / FAIL | PB2 / PC5 | 候选低有效，高关闭 |
| FAULT | PC2 | 候选高有效，低关闭 |
| DE / 低有效 RE | PB1 | 候选并接，低接收，高发送 |
| 调试 UART | USART1 PA9/PA10 | 115200、8N1；交付版不接收注入命令 |
| RS485 UART | USART2 PA2/PA3 | 115200、8E1；HAL 9 位字长含偶校验，8 位有效数据 |
| USART2 RX DMA | DMA1 Stream5 Channel4 | 环形 512 B，HT / TC / 错误 IRQ |
| USART2 TX DMA | DMA1 Stream6 Channel4 | 普通模式，完整响应不超过 256 B |
| 微秒时间 | TIM2 | 1 MHz、32 位自由运行，PSC83 |
| RX 活动检测 | TIM3 | 50 μs，PSC83、ARR49 |
| HAL / RTOS 时基 | TIM6 / SysTick | 1 ms / 1 ms |
| IWDG | LSI、预分频32 / reload2999 | 名义 32 kHz 下约 3 s |

USART1、USART2、DMA1 Stream5/6、TIM3 IRQ 优先级均为 6，可以调用 FreeRTOS FromISR；最大系统调用 IRQ 优先级为 5。TIM6 和 SysTick 保留优先级 15。ISR 只记录数据、时间、错误、TX 完成并通知任务，不执行工位命令或格式化日志。

接线与有效电平集中在 Bsp/Board/board_config.h。GPIO 先写关闭电平，再配置输出；输出变化先关旧结果再开新结果，同一输出不重复切换。Fatal 路径尽可能撤销 PASS/FAIL、置 FAULT、关闭 DE 并停止运行，由已启动的 IWDG 恢复。上电代码执行前的状态取决于外部电路，仍须实测。

当前 `BOARD_WIRING_CONFIRMED=0` 仅是未确认标记，不自动阻止离线构建。板卡与引脚冲突、灯极性或收发器要求不同，应先修改配置再烧录。`.ioc` 只参考 GPIO/时钟，手工驱动和原生 RTOS 不依赖再生成。

## 执行、并发和保护

| 任务 | 周期 / 优先级 | 静态栈（32 位 word） | 正常循环结束时更新 |
|---|---|---|---|
| CommTask | 通知唤醒，最长等待10 ms / 4 | 768 | 通信进度、栈水位 |
| WorkstationTask | 10 ms / 3 | 512 | 工位进度、栈水位 |
| HealthTask | 100 ms / 5 | 256 | 健康进度、栈水位；健康时 MCU_HEARTBEAT |

命令队列静态容量 8，存储完整命令及安全代次。状态、ACK、事件和 GPIO 在统一短临界区内提交，时间在临界区内重新获取，避免更高优先级任务推进时间戳后误判。快照从副本编码；串口发送和日志格式化在临界区外。

故障或新 SYNC 推进安全代次，旧排队命令明确返回状态拒绝，合法拒绝也缓存；它们不能在健康恢复后因重传而执行。完全相同的最近命令重传只发布历史 ACK，不再次操作 GPIO。同 epoch/seq、不同内容返回 ACK=4，不覆盖原缓存。普通 seq 严格前进，允许跳号；更大非零 epoch 才能建立新同步周期。

CommTask 每轮最多取 128 条接收记录并输出 2 条日志，每轮至少阻塞一个 tick，给工位任务运行机会。RX 环 1024 条记录，包含 1024 个数据字节容量及每字节微秒时间/类型元数据，实际占 8192 B；与 RX DMA 的 512 B 分开。所有 DMA 缓冲位于普通 SRAM，无 CCM。

发生 UART PE/FE/NE/ORE、DMA 错误、接收记录溢出或无法可靠恢复时间信息时，记录错误、清理受影响帧并重启接收。检测中或保持结果时严重 RX 错误触发 FAULT_RX。接收重启失败会在后续通信循环继续尝试，不阻塞任务。

RTU 初值 t1.5=750 μs、t3.5=1750 μs。字节记录时间来自 ISR 的 DMA 进度检查；间隔计算扣除一个 96 μs 字符时间。静默定界额外等待字符时间及 50 μs 监视余量，避免把尚未 DMA 完成的字节分到下一帧。任务延迟不替代接收时间。50 μs 采样带来边界误差，靠近阈值的物理分帧与最坏 ISR 延迟仍须实测，当前软件测试不是波形验收。

完整响应准备后置 DE，启动 TX DMA；DMA 完成只启用 USART TC IRQ，最后停止位完成后释放 DE。发送期间丢弃本机 RX 回波，推荐在候选接线确认后用 DE 并接低有效 RE 关闭接收器。50 ms TX 看门限超期或 DMA 错误会中止发送并走通信故障恢复，不将其标为成功快照。

## 寄存器及业务协议

从站 1；广播不执行不响应；CRC 初值 FFFF、多项式 A001、低字节先发。寄存器字节高位先发，32 位字段高字在低地址。

FC04 支持下表连续子区间，完整快照 0x0000 起 22 字。FC06 仅写 0x0100 心跳；FC10 从 0x0110 一次写 9 字 / 18 字节，依次为 epoch_H/L、token_H/L、seq_H/L、opcode、arg_H/L。FC03 可读心跳单字或 0x0110～0x0118 连续子区间，返回最近已处理且缓存的命令，包括明确业务拒绝。地址非法异常2、数量/值非法异常3、不支持功能异常1、命令队列满异常6；CRC错误/非本机地址/物理畸形帧丢弃。

| 地址 | 字段 |
|---|---|
| 0000 | STATE：BOOT0、IDLE1、READY2、INSPECTING3、COMPLETED4、FAULT5 |
| 0001 | INPUT_FLAGS：到位bit0、ACTION按下bit1、RESET按下bit2，均去抖逻辑值 |
| 0002 | OUTPUT_FLAGS：仅0、PASS1、FAIL2、FAULT4，反映实际提交逻辑值 |
| 0003 | FAULT_CODE：无0、超时1、检测离位2、RX3、任务4、撤销5、未同步6 |
| 0004 | HEALTH_MASK：Comm bit0、Work bit1、Health bit2，正常7 |
| 0005 | MCU_HEARTBEAT：一次正常内部健康检查后递增 |
| 0006/0007 | 本次启动运行毫秒数高/低字，允许32位回绕 |
| 0008/0009 | 活动任务token高/低字 |
| 000A/000B | ACK命令seq高/低字 |
| 000C | ACK_STATUS：无0、成功1、状态2、token3、epoch/seq4、健康5、参数/命令6 |
| 000D/000E | INPUT_EVENT_SEQ高/低字 |
| 000F | EVENT_CODE：无0、ACTION1、RESET2 |
| 0010 | RESET_REASON：上电bit0、引脚bit1、软件bit2、IWDG bit3、WWDG bit4、低功耗bit5、BOR bit6 |
| 0011/0012 | COMM_ERROR_COUNT高/低字，32位饱和 |
| 0013 | 协议版本0200 |
| 0014/0015 | ACK epoch高/低字 |

上位机不能把 FC10 写入成功视作业务成功，须继续读取 ACK 身份、状态、token 和输出。START要求 READY/到位/非零token/新鲜心跳/全部健康；RESULT要求 INSPECTING/同token/到位/健康，ARG仅1或2。RESET仅FAULT、token与arg为0，原因可解除且心跳与内部健康恢复才接受。ACK_EVENT精确匹配锁存事件，不约束任务token。ABORT不要求新鲜心跳，有当前token时须匹配，token已清零时允许原任务撤销。

新 SYNC 保留最近心跳数值，但清除有效资格；必须收到再次变化的值后才能 RESET/START。正常完成离位清token与结果进入IDLE；检测中离位锁存FAULT。任务异常、RX故障、检测离位、主站超时、撤销、未同步按此顺序确定优先级，另保留首次故障时间和token。恢复心跳或重新到位不会恢复历史PASS。

## 按钮操作

10 ms 采样，连续稳定满30 ms更新；只有按下沿产生事件，松开不清除、长按不重复。同周期两按钮均按下时 ACTION 先生成事件，RESET 计入忙碌。已有事件或未重新开放时的新按键丢弃并计数，不覆盖旧事件。

ACK清事件后，须完成一帧包含对应成功ACK和空事件的完整FC04响应，等待自ACK执行起至少1200 ms，并且两按钮均去抖释放，才重新开放。部分快照、发送失败或其他命令ACK不满足条件；保持期按键不缓存。连续测点按钮应留足这个保持期。事件序号在一次启动中不复用，SYNC只清事件内容，序号保留；耗尽或安全代次耗尽时关闭结果并受控复位。

## 健康与看门狗

心跳只有16位值变化才续期，FFFF→0000有效，相同值只能回显。初值超时1500 ms，超时清token和结果并锁存FAULT。通信与健康任务进度期限300 ms，工位100 ms，ISR不能替任务报健康。HealthTask完成检查且内部全部健康才喂IWDG；外部主站离线但内部任务健康仍喂狗，避免复位风暴。

工位任务也检查健康与心跳，因此 HealthTask停滞仍能由工位路径关闭结果，再由IWDG复位。三个任务和队列静态创建；栈溢出、分配失败、断言及CPU致命异常先尽可能置安全故障输出，停止执行和喂狗。

IWDG名义3 s不是实测值，实际由LSI频率决定。心跳保护目标不超过最后变化被接受后1700 ms，需要用波形验证实际输出撤销时间。

## 诊断与注入

USART1 输出形如 `v=1.0.0 t=123 code=3 a=7 b=4 c=196609`。字段为十进制，t为HAL毫秒。诊断队列32条，满时丢新记录并计数；串口TX环1024 B，整条日志不足空间时丢弃，不阻塞控制。

| code | a / b / c |
|---|---|
| 1 启动 | 原始RCC_CSR / RESET_REASON / 接线确认标记 |
| 2 状态 | 旧状态 / 新状态 / 当前token |
| 3 成功命令、4拒绝 | epoch / seq / `(opcode<<16)|ACK_STATUS` |
| 5 故障 | 当前最高原因 / 首故障token / 首故障毫秒 |
| 6 输出 | 旧输出 / 新输出 / 当前token；t为提交撤销时刻 |
| 7 接收异常 | 本次数量 / 工位状态 / 0 |
| 8 栈水位 | Comm / Work / Health剩余word，每约10 s记录 |
| 9 时钟 | SYSCLK / PCLK1 / PCLK2（软件计算值） |
| 10 命令参数 | 紧随命令身份记录：请求token / arg / opcode |
| 11 按钮事件 | event_seq / event_code / 去抖inputs |

SWD可观察 g_workstation、g_task_cycles[Comm,Work,Health]、g_stack_watermark、诊断队列丢弃及UART丢弃数。COMM_ERROR_COUNT仅通信/协议类错误；button_busy、business_rejects独立保存在g_workstation。

仅 `debug-inject` 构建通过 USART1 8N1 接收以下换行文本：

```text
inputs 0
inputs 1
inputs 3
inputs auto
pause comm 1
pause comm 0
pause work 1
pause work 0
pause health 1
pause health 0
rx_fault 1
rx_fault 0
diag
```

inputs使用逻辑位0～7，仍经过正常去抖；暂停任务不推进进度，健康任务暂停不喂狗。diag显示队列丢弃、栈水位和任务循环数。交付版不仅宏关闭，还校验二进制中没有这些注入命令入口字符串。

## 离线测试证据和边界

CTest 8组：复位128种组合；8条交接黄金向量及Modbus访问；状态/身份/拒绝/旧队列；去抖/按钮重新开放/心跳和毫秒回绕；RTU/环形/DMA游标；压力；上游C++契约；独立进程回放。

压力测试含100000个随机ADU、1000次Health/RESULT串行化交错、10000次虚拟工位闭环。上游C++契约覆盖1/5点按钮模式、PASS/FAIL、ACK、ABORT和故障后历史结果重传；它使用真实上游协议函数，但不是完整Linux服务、BLE或物理串口验收。独立进程回放使用生产纯C模块和虚拟时间，运行真实耗时与虚拟2880 ms分别记录。

报告不把虚拟循环数或虚拟时间写成24 h实机运行。硬件验收项目详见09；本版本可继续上板联调，未形成最终实机闭环证据。
