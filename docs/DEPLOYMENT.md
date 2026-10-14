# 部署与运维

## 上位机配置

在 Linux 安装 [上位机依赖](../host/README.md)，构建 `inspection_service`。运行用户需要 BlueZ 系统 D-Bus 访问、蓝牙控制器访问、串口权限与数据目录写权限。

编辑 [`host/config/hardware.yaml`](../host/config/hardware.yaml)：

- `instrument.device_name`：实际测厚仪 BLE 名称或标识。
- `workstation.device`：优先使用稳定的 `/dev/serial/by-id/...` 设备路径。
- 串口参数：115200、even、地址 1。
- `products`：实际点数、厚度上下限、FE / NFE 与规则版本。
- `database.path`：持久化目录，默认相对配置目录解析。
- `testing.allow_controls`：现场配置为 `false`。

使用自动方向控制的 USB-RS485 转换器。蓝牙仪器与串口由当前服务独占。前台启动：

```sh
./host/build/inspection_service --config host/config/hardware.yaml
```

## MCU 板级配置

入口为 [`board_config.h`](../mcu/Bsp/Board/board_config.h)。按板卡原理图适配引脚、电平与 HSE，输入寄存器使用逻辑有效状态。

| 功能 | 默认引脚 / 资源 | 配置 |
|---|---|---|
| 工件到位 | PA0 | 高有效，内部下拉 |
| ACTION | PA1 | 低有效，内部上拉 |
| RESET | PA4 | 低有效，内部上拉 |
| PASS | PB2 | 低有效，关闭电平为高 |
| FAIL | PC5 | 低有效，关闭电平为高 |
| FAULT | PC2 | 高有效，关闭电平为低 |
| RS485 DE | PB1 | 低接收，高发送 |
| RS485 UART | USART2，PA2 / PA3 | 115200 / 8E1 |
| 调试 UART | USART1，PA9 / PA10 | 115200 / 8N1 |
| RX DMA | DMA1 Stream5 Channel4 | 512 B 环形缓冲 |
| TX DMA | DMA1 Stream6 Channel4 | 普通 DMA |
| 时间与监视 | TIM2 / TIM3 | 1 MHz 时间戳 / 50 μs 接收监视 |
| MCU 时钟 | 25 MHz HSE | 168 MHz SYSCLK |

固件使用 SWD 烧录，命令见 [构建与烧录](../mcu/CMAKE_BUILD.md)。将上位机与 MCU 的 RS485 A / B 连接，地参考、终端匹配和偏置按收发器与总线拓扑配置。

## 服务部署

[`inspection.service.example`](../host/deploy/inspection.service.example) 提供 systemd 模板。安装前设置专用用户、程序与配置绝对路径，将产品配置及回放文件按相对路径关系部署。

```sh
sudo install -m 755 host/build/inspection_service /usr/local/bin/inspection_service
sudo mkdir -p /etc/coating-inspection /var/lib/coating-inspection
sudo cp host/config/hardware.yaml /etc/coating-inspection/hardware.yaml
```

将配置中的数据库路径设为 `/var/lib/coating-inspection/inspection.db`，为服务用户授予目录权限；将修改后的服务模板保存为 `/etc/systemd/system/coating-inspection.service`。

```sh
sudo systemctl daemon-reload
sudo systemctl enable --now coating-inspection.service
systemctl status coating-inspection.service
journalctl -u coating-inspection.service -f
```

模板使用 `dialout` 附加组、失败重启与 journald。按设备容量设置日志保留策略，避免长期日志占满存储。

## 运行检查与诊断

服务启动后检查仪器与工位连接、`sync_complete`、工件到位以及 RESET / START 的 `command_done`。检测结束分别检查 `inspection_completed` 与结果命令 ACK，使用 `query <workpiece_id>` 查询点位和结果。

日志为逐行 JSON，包含 `event`、`message`、`wall_ms` 及业务字段。按任务、候选、epoch / 序号定位操作；MCU USART1 输出版本、时间、事件码与参数。

数据库使用 WAL。备份可使用 SQLite 的在线备份接口，或停止服务后保存完整数据库文件。对时间追溯配置系统校时，并保留记录中的时间同步状态。

串口诊断工具见 [MCU 使用说明](../mcu/README.md)。切换为诊断主站前停止占用同一串口的服务。同一 RS485 总线保持一个活动主站。
