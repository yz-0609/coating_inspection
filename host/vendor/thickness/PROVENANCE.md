# BLE 组件来源与适配

来源：[Thickness_bluetooth_protocol](https://github.com/yz-0609/Thickness_bluetooth_protocol)

固定提交：`7db6610bd7cdd23348475f12dbd8ee7846105f4f`。

本工程保留组件的 `include/` 与 `src/`，由上位机 CMake 定义构建目标。

- `Measurement::milli_um` 以整数表示千分之一微米厚度，由解码后的符号与幅值换算；保留 `micrometers` 接口。
- `ReaderCallbacks` 增加结构化 `diagnostic` 回调，适配层累计 CRC 与格式错误计数。
- 上位机适配层补充接收时间、连接代次、本地序号和候选编号。
- 连接变化时清除协议半帧，防止跨连接组帧。

协议解析回归用例位于 `host/tests/unit.cpp`。更新组件时同步维护来源记录，并执行上位机测试。
