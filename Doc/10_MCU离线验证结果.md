# MCU 离线验证结果

固件版本：1.0.0。
检查 UTC 时间：2026-10-07T12:07:00.095597+00:00。
本次全量检查实际耗时：14.313 秒。

## 构建与产物

| 变体 | 源文件 | BIN字节 | 应用警告 | 依赖警告 | 注入 |
|---|---:|---:|---:|---:|---|
| debug | 52 | 51684 | 0 | 5 | 关闭 |
| release | 52 | 51684 | 0 | 5 | 关闭 |
| debug-inject | 52 | 53612 | 0 | 5 | 开启 |

ELF/BIN/HEX一致，HEX校验、启动向量、ARM ABI、段权限、未解析符号、官方依赖哈希、普通SRAM缓冲和注入隔离检查通过。

交付BIN SHA-256：`b02e1f66391bc2c3782b40abb38db7352fe14e2c4010809a3c0e32b298b30a08`。

## 自动测试

CTest：8/8 通过。

- reset_reason
- modbus
- workstation
- inputs_health
- transport
- pressure
- upstream_host_contract
- offline_replay

压力：100000个随机ADU、1000次保护/结果交错、10000次虚拟工位闭环。
上游契约：未修改的C++编码、解码及command_satisfied，提交 `aab161d065bab147a8894b28832d413973821423`。
独立进程回放：65 个事务/步骤，虚拟时间2880 ms，实际耗时另记JSON。

## 证据位置和未验证项目

build-validation 保存完整构建日志、ctest.xml、summary.json及offline-replay.json。
固件与源代码包位于 delivery；SHA256SUMS.json记录交付文件哈希。

未连接或烧录真实MCU；GPIO、电平、时钟、UART/DMA/DE波形、Linux完整服务、真实BLE仪器、实际保护延迟、栈余量和实机24 h未验收。
这些结果仅为离线软件验证，不能作为物理现场或24 h报告。
