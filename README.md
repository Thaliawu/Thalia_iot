# 基于 ESP32 的工业级环境监测与边缘计算网关

> 支持 Modbus RTU/TCP · FreeRTOS 多任务 · EEPROM 掉电保护 · 电源抗干扰测试

本项目模拟工业现场温湿度采集节点，基于 **ESP32 + FreeRTOS** 实现实时采样、本地边缘判断与工业协议上报，并扩展了 **Spring Boot 数据管道**，适用于智能制造及新能源场景。

---

## 📌 项目总览

| 阶段 | 内容 | 技术栈 | 状态 |
|------|------|--------|------|
| 第1-4周 | 嵌入式固件与 Modbus 协议 | ESP32, FreeRTOS, Modbus TCP, CRC16, EEPROM, WiFi 重连 | ✅ 完成 |
| 第5周 | PC 端环境搭建 | VMware Ubuntu, Docker, Java 17, Maven | ✅ 完成 |
| 第6周 | 数据管道搭建 | Spring Boot, JPA, PostgreSQL, Python 模拟器, 定时聚合 | ✅ 完成 |
| 第7-12周 | 可视化、告警、微服务、MQTT、K8s | 进行中... | 🚧 开发中 |

---

## 🔧 已完成模块

### 1. ESP32 Modbus TCP 从站（嵌入式）
- FreeRTOS 双任务架构（采样 + 通信）
- 原生实现 Modbus TCP 协议（功能码 03，异常码 01/02/03）
- EEPROM 掉电保护（每30秒保存，CRC16 校验）
- WiFi 断线重连（指数退避）
- 硬件看门狗（30秒）
- 环形缓冲区存储最近 10 条传感器数据
- LED 状态指示

**测试验证**：
- ✅ 压力测试（200ms 间隔，成功率 100%）
- ✅ 非法地址/功能码/数量 → 标准异常码
- ✅ 拔插电源 → EEPROM 恢复数据
- ✅ WiFi 干扰 → 自动重连

### 2. 数据采集后端（Spring Boot）
- Spring Boot 3.4.1 + JPA + PostgreSQL
- REST API `POST /api/data` 接收 JSON 数据
- 每分钟定时聚合温湿度平均值（`@Scheduled`）
- Python 模拟器持续发送数据

**运行效果**：
```bash
curl -X POST http://localhost:8080/api/data -H "Content-Type: application/json" -d '{"temperature":25.6,"humidity":68.2}'
# 返回: Data inserted successfully
📸 运行状态
ESP32 在 FreeRTOS 调度下稳定运行，串口持续输出温湿度数据：

![ESP32输出图](ESP32-output.png)

📁 仓库结构
text
Thalia_iot/
├── src/                       # ESP32 固件代码（Modbus TCP 从站）
├── tests/                     # Python 测试脚本
│   ├── raw_test.py            # 原生 socket Modbus 读取
│   ├── stress_test.py         # 压力测试
│   └── test_exception.py      # 异常报文验证
├── backend/                   # Spring Boot 数据采集服务（可自行创建目录）
├── docs/                      # 文档与报告
│   ├── Protocol_Commissioning_Report.md
│   ├── Reliability_Test.md
│   └── images/                # 测试截图
└── README.md
🚀 快速开始（后端部分）
bash
# 1. 安装 PostgreSQL（Ubuntu）
sudo apt install postgresql
sudo -u postgres psql -c "CREATE USER myuser WITH PASSWORD 'mysecretpassword';"
sudo -u postgres psql -c "CREATE DATABASE iot_db OWNER myuser;"

# 2. 启动 Spring Boot 应用（进入 backend 目录）
mvn spring-boot:run

# 3. 运行模拟器（另开终端）
python3 modbus_simulator.py

# 4. 查询数据库
psql -h localhost -U myuser -d iot_db -W -c "SELECT * FROM device_data ORDER BY create_time DESC LIMIT 5;"
📚 详细报告
Modbus 协议联调验收报告

系统可靠性测试报告

🧭 后续计划
第7周：Grafana 可视化 + 邮件告警

第8周：微服务拆分 + Docker Compose

第9周：MQTT 设备接入（EMQX）

第10周：Kafka + OPC UA 桥接

第11周：Kubernetes 部署

第12周：简历与面试准备

