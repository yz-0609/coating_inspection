# Device 层

DigitalInput 的去抖和 Output 的门控策略为纯 C，可在原生工程测试。
OutputManager 只在应用共享状态的短临界区内提交 GPIO，先关旧结果再开新结果，不重复切换未变化的输出。RS485 设备收发由 Bsp/RS485 和 Module/Transport 联合封装。
电平和候选接线集中于 Bsp/Board/board_config.h，业务状态机不依赖 HAL。
