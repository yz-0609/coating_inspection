# MCU 实机验收与已知限制

当前状态：软件已交付，用户当前无硬件，以下项目均未执行。取得硬件后按清单补证据，不以离线测试代替。

## 接线与上电

- 确认 STM32F407VET6、25 MHz 晶振、板卡版本；核对候选 PA0/PA1/PA4、PB1/PB2、PC2/PC5、PA2/PA3、PA9/PA10 的引出、复用、极性和电平。
- 确认RS485收发器型号、供电、DE/低有效RE能否并接、终端和偏置；记录开关机、NRST时外部上下拉与灯态。
- 确认SWD调试器，使用交付ELF烧录并校验。保存固件哈希、构建日志和实际板卡接线图。
- 波形确认上电、NRST、软件复位、IWDG复位均不恢复历史结果；日志确认复位掩码和原始RCC标志，多标志不丢失。

## 串口与时序

- 实测 USART2 115200、8E1，包含80～FF高位数据字节；不能只凭HAL配置判定通过。
- 检查DMA半满、全满、跨界和持续负载；注入PE/FE/NE/ORE、DMA错误、软件环溢出，确认丢弃受影响帧后接收恢复。
- 测量750/1750 μs边界附近的帧内/帧间间隔。TIM3采样误差与中断延迟可能导致边界附近合法帧保守丢弃，需结合实际主站事务间隔修订或验证。
- 正常响应目标20 ms内，必须满足上位机150 ms期限；测量满负载最坏时间。
- DE波形必须覆盖最后停止位，DMA TC不能提前释放；验证回波屏蔽与下一请求可接收。

## 输入、业务和异常

- 短按、长按、接点抖动、同周期同时按、事件未ACK时再按；检查事件保持与忙碌计数。
- ACK后完成完整空事件快照，1200 ms保持期且按钮释放后才接收下一事件；保持期按键不补发。
- 完整闭环：上电 → SYNC →变化心跳 →人工RESET →READY →START →INSPECTING →PASS/FAIL →COMPLETED →正常离位 →IDLE。
- 检测中去抖到位失效后，下一工位周期撤销结果并故障；完成后正常离位清结果和token。
- 相同epoch/seq内容重传幂等；冲突、旧epoch/seq/token、明确拒绝重传均不能产生新动作。
- PASS成功后断RS485，再发原RESULT；只能历史ACK成功，不能恢复PASS。新SYNC不能消除未恢复内部故障，RESET需要新心跳和所有健康条件。
- 断主站、退出/强杀主站；从最后变化心跳接受时刻测实际输出关闭，初值目标≤1700 ms；恢复通信不自动恢复结果。

## 任务、日志与看门狗

- 显式故障注入版分别pause comm/work/health；记录保护时间、故障码、GPIO、IWDG复位与启动原因。
- 使用真实执行顺序或调试器断点制造Health/RESULT交叉，观察故障门控始终覆盖结果提交。
- 测量实际LSI/IWDG期限，记录调试器是否冻结IWDG；三个静态任务栈256/768/512 words为初值，读取水位并压测余量。
- 外部离线而任务健康时保持FAULT、持续条件喂狗，没有复位风暴。
- 日志队列和UART拥塞时验证控制不阻塞、丢弃计数增长；ISR无格式化打印。

## 主站、真实仪器和长稳

在现有Linux主机使用真实Modbus配置替代模拟工位，先CLI再实体按钮；关联host_epoch、command_seq、token、snapshot和GPIO。上位机实际服务未在本Windows环境执行，当前C++契约测试不能替代这一步。

CLI单条操作示例（Python串口工具需要 `python -m pip install -r tools/requirements.txt`，仅实际串口使用）：

```powershell
python tools/mcu_cli.py --port COM5 snapshot
python tools/mcu_cli.py --port COM5 sync --epoch 1 --seq 1
python tools/mcu_cli.py --port COM5 heartbeat 1
python tools/mcu_cli.py --port COM5 reset --epoch 1 --seq 2
python tools/mcu_cli.py --port COM5 start --epoch 1 --seq 3 --token 99
python tools/mcu_cli.py --port COM5 heartbeat 2
python tools/mcu_cli.py --port COM5 pass --epoch 1 --seq 4 --token 99
python tools/mcu_cli.py --port COM5 abort --epoch 1 --seq 5 --token 99
```

COM5只是示例，epoch/seq需取当前合法值，人工操作之间间隔不得忽略1500 ms心跳期限。CLI没有后台续期或自动RESET。与生产主站同时操作会造成双主站冲突，应在独立联调期间使用。

真实BLE测厚仪仍由上位机连接，MCU不接BLE。验证1点/5点、同值测量、边界、材料不匹配、仪器断连；区分质量FAIL与现场UNKNOWN/FAILED。再执行RS485断线、主站强杀、检测中离位、MCU复位和短时故障回归。

真实串口寄存器长稳记录工具：

```powershell
python tools/hardware_soak.py --port COM5 --hours 24 --output hardware-tests/serial-soak.json
```

此工具为唯一主站发送变化心跳并读取快照，生产服务应停止。它记录真实墙钟运行秒数、是否达到请求时长、读写次数、错误、复位观察和MCU正常检查次数；中断或失败报告completed=false并非零退出。它不执行检测任务，inspection_sessions=0明确注明；GPIO、真实仪器和整机生产任务24 h仍需单独运行、计数并保存证据。当前未运行该工具，不存在24 h硬件报告。

## 当前限制

1. 候选电气配置未经实物确认，默认全部沿用计划；修改board_config后需重新构建、回归和更新接线表。
2. GPIO无独立物理反馈，OUTPUT_FLAGS证明软件提交逻辑值，不能证明灯/执行器实际动作。
3. RX时间来自DMA活动采样，有50 μs粒度及ISR延迟；靠近边界的时序、最大临界区耗时和中断负载必须实测。
4. 栈大小、1500 ms保护参数和名义3 s IWDG尚未按实际硬件测量。当前交付版IWDG已启用，上板调试应考虑复位。
5. 默认从站地址/波特率/接线/期限为编译时参数，不提供远程配置寄存器或现场固件升级。
6. 没有真实Linux服务+MCU+BLE完整闭环和实际24 h证据；离线测试通过仅作为软件交付依据。

最终验收归档至少包含：板卡/接线/固件与主站配置版本，ELF/BIN哈希，SWD记录，串口及DE波形，保护延迟、栈水位，正常与故障工位日志，真实仪器场景，24 h实际时长、检测任务次数、错误和是否完成。
