# 🏭 基于 ESP32 的工业级环境监测与边缘计算网关

> 支持 Modbus RTU/TCP · FreeRTOS 多任务 · EEPROM 掉电保护 · 电源抗干扰测试

本项目模拟工业现场温湿度采集节点，基于 **ESP32 + FreeRTOS** 实现实时采样、本地边缘判断与工业协议上报，适用于智能制造及新能源场景。

---

## 📷 运行状态

ESP32 在 FreeRTOS 调度下稳定运行，串口持续输出温湿度数据：

![ESP32输出图](ESP32-output.png)

---

## 🛠️ 技术栈

| 分类 | 技术 |
|----|----|
| MCU | ESP32-D0WDQ6（双核 Xtensa LX6） |
| OS | FreeRTOS（Queue / Semaphore / Task） |
| 协议 | Modbus RTU（CRC16）、Modbus TCP |
| 传感器 | DHT11 / DHT22（温湿度） |
| 存储 | EEPROM（参数掉电保护） |
| 通信 | UART / RS485 / LWIP |
| 工具链 | PlatformIO + VS Code |

---

## ⚙️ 功能特性

- ✅ **多任务架构**：采样 / 通信 / 看门狗任务解耦
- ✅ **Modbus 寄存器模型**：Holding Register 映射温湿度
- ✅ **边缘计算**：超阈值本地缓存，不断网丢数
- ✅ **掉电保护**：关键参数 EEPROM 存储，上电校验
- ✅ **抗干扰**：通过电源热插拔测试，任务自动恢复
- ✅ **可扩展**：预留 MQTT / OPC UA / SCADA 接口

---

## 📐 Modbus 寄存器定义（示例）

| 地址 | 功能 | 说明 |
|----|----|----|
| 40001 | 温度 ×10 | uint16 |
| 40002 | 湿度 ×10 | uint16 |
| 40003 | 设备状态 | bit0=在线 bit1=告警 |

---

## 🏭 应用场景

- 苏州工业园区 · 智能制造产线环境监测
- 新能源储能柜温湿度预警
- 工业设备预测性维护（边缘节点）

---

## 📁 项目结构
├── src/                # 主程序（FreeRTOS 任务、Modbus）

├── include/            # 头文件（寄存器定义、宏）

├── images/             # 项目截图

│   └── ESP32-output.png

├── platformio.ini      # ESP32 配置 & 依赖

├── README.md           # 项目说明（本文件）

└── .gitignore

---

## 📌 当前状态

✅ 代码稳定  
✅ Modbus RTU 自研实现  
✅ 通过电源拔插干扰测试  
✅ README & 文档完善  
