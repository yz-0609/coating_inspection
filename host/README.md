# Linux 上位机使用说明

上位机负责 BLE 测量采集、检测任务、产品规则、质量判定、SQLite 追溯与工位通信。项目整体介绍见 [仓库主页](../README.md)，系统说明见 [架构](../docs/ARCHITECTURE.md)与[协议](../docs/PROTOCOL.md)。

以下命令在 `host/` 目录运行。

## 构建

Ubuntu / Debian 安装依赖：

```sh
sudo apt install build-essential cmake pkg-config libglib2.0-dev libsqlite3-dev \
  libyaml-cpp-dev libmodbus-dev nlohmann-json3-dev python3
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j4
ctest --test-dir build --output-on-failure
```

## 自动演示

```sh
python3 tools/demo.py --binary build/inspection_service --output reports/demo-pass
python3 tools/demo.py --binary build/inspection_service --result fail --output reports/demo-fail
python3 tools/demo.py --binary build/inspection_service --result aborted --output reports/demo-aborted
```

默认连接独立 PTY 工位模拟器，使用 libmodbus 主站执行五点流程。`--backend mock` 可切换内存工位。输出目录保存 SQLite 与 `summary.json`，每次选择新目录。

## 交互检测

```sh
./build/inspection_service --config config/demo.yaml
```

等待 `sync_complete`，按顺序输入：

```text
sim present 1
reset
status
start WORKPIECE-001 demo5
arm
```

RESET 与 START 操作等待 `command_done` 成功后继续。收到 `candidate` 日志后，使用其中的编号执行 `confirm <candidate_id>`；出现 `point_saved` 后为下一点重新 `arm`。五点完成时检查 `inspection_completed` 和结果命令的 `command_done`。

| 命令 | 作用 |
|---|---|
| `status` | 当前任务、流程、连接与工位状态 |
| `start <workpiece_id> <product_id>` | 建立检测任务 |
| `arm` | 开启当前点采集窗口 |
| `confirm <candidate_id>` | 确认候选并保存点位 |
| `cancel` | 中止当前任务 |
| `reset` | 请求复位工位故障 |
| `query <workpiece_id>` | 查询工件检测历史 |
| `outbox` | 查看本地完成事件 |
| `export <path>` | 导出事件 JSON |
| `quit` | 收尾业务并退出 |

工件编号支持双引号包含空格。复检使用相同工件编号，生成新任务编号。`demo1` 与 `demo5` 分别为单点和五点产品，示例阈值为 100～200 μm、FE 基材；部署时使用实际工艺规则。

## 独立工位模拟器

```sh
python3 tools/workstation_sim.py
```

模拟器输出 `/dev/pts/N`。在 `config/` 中复制配置，将 `workstation.type` 设为 `modbus`，设备设为实际 PTY 路径。通过模拟器标准输入发送 JSON 控制：

```json
{"present":true}
{"button":"action"}
{"button":"reset"}
{"reset_mcu":true}
{"drop_ack":true}
{"ack_delay_ms":500}
{"reject_opcode":3}
{"drop_responses":1}
{"drop_ack":false,"ack_delay_ms":0,"reject_opcode":0}
```

`{}` 查询状态，`{"quit":true}` 退出。`--scenario <file>` 可加载带 `at_ms` 的控制数组。

## 运行配置

`config/hardware.yaml` 使用 BLE 仪器与 Modbus 工位；`config/demo.yaml` 使用回放仪器和内存工位。配置文件中的相对路径以配置文件所在目录解析。

| 配置 | 用途 |
|---|---|
| `instrument.device_name` | BLE 名称或设备标识 |
| `workstation.device` | 串口设备路径 |
| `workstation.poll_ms` | 状态轮询周期，默认 100 ms |
| `workstation.heartbeat_ms` | 心跳周期，默认 200 ms |
| `workstation.command_timeout_ms` | 业务确认期限，默认 1000 ms |
| `database.path` | SQLite 文件位置 |
| `products` | 点数、厚度上下限、基材与规则版本 |
| `testing.allow_controls` | 开发配置的模拟控制开关 |

产品规则支持 1～5 点、FE / NFE，厚度配置最多三位小数。硬件配置关闭模拟控制，并由操作员通过 CLI 或现场按钮完成流程。

## 开发与运行监测

开发配置支持 `sim measure 150 FE`、`sim connection 0`、`sim stall 2500`、`sim overflow` 等控制。`sim measure` 经过仪器帧编码与解析。完整测试入口见 [验证指南](../docs/TESTING.md)。

```sh
python3 tools/soak.py --binary build/inspection_service \
  --duration 86400 --interval 30 --output reports/soak-24h
```

报告记录请求与实际时长、流程完成状态、PASS / FAIL / 中止数量、资源采样和一致性检查。

部署说明与日志维护见 [部署指南](../docs/DEPLOYMENT.md)，BLE 组件来源见 [PROVENANCE.md](vendor/thickness/PROVENANCE.md)。
