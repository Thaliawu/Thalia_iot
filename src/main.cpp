#include <Arduino.h>
#include <WiFi.h>
#include "DHT.h"

// ========== DHT11 配置 ==========
#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// ========== WiFi 配置 ==========
const char *ssid = "Thalia";        // 改成你的 WiFi 名
const char *password = "ssjw12138"; // 改成你的 WiFi 密码

// ========== Modbus TCP 配置 ==========
WiFiServer modbusServer(502); // Modbus TCP 默认端口 502
WiFiClient client;            // 当前连接的客户端

// 保持寄存器区（Holding Registers）
// 地址 0: 温度值（放大10倍，如 25.6℃ -> 256）
// 地址 1: 湿度值（放大10倍，如 68.0% -> 680）
uint16_t holdingRegisters[2] = {0, 0};

// ========== 更新传感器数据到寄存器 ==========
void updateSensorData()
{
    float temp = dht.readTemperature();
    float hum = dht.readHumidity();

    if (!isnan(temp) && !isnan(hum))
    {
        holdingRegisters[0] = (uint16_t)(temp * 10); // 温度 *10
        holdingRegisters[1] = (uint16_t)(hum * 10);  // 湿度 *10

        Serial.print("温度: ");
        Serial.print(temp);
        Serial.print("℃ -> 寄存器0 = ");
        Serial.print(holdingRegisters[0]);
        Serial.print("  湿度: ");
        Serial.print(hum);
        Serial.print("% -> 寄存器1 = ");
        Serial.println(holdingRegisters[1]);
    }
    else
    {
        Serial.println("DHT11 读取失败，寄存器保持不变");
    }
}

// ========== 发送 Modbus 响应报文（带调试打印）==========
void sendModbusResponse(uint8_t *response, int len)
{
    Serial.print("准备发送响应，长度: ");
    Serial.println(len);

    if (client.connected())
    {
        int sent = client.write(response, len);
        client.flush();
        Serial.print("实际发送字节数: ");
        Serial.println(sent);

        // 可选：打印发送的前几个字节（十六进制）
        Serial.print("响应报文前8字节: ");
        for (int i = 0; i < (len > 8 ? 8 : len); i++)
        {
            Serial.print(response[i], HEX);
            Serial.print(" ");
        }
        Serial.println();
    }
    else
    {
        Serial.println("错误: client 已断开连接，无法发送响应");
    }
}

// ========== 处理 Modbus TCP 请求（只实现功能码 0x03）==========
void handleModbusRequest(uint8_t *request, int len)
{
    // 调试打印：收到请求
    Serial.print("收到 Modbus 请求，长度: ");
    Serial.println(len);

    // Modbus TCP 报文最小长度：MBAP(7) + PDU(2) = 9 字节
    if (len < 9)
        return;

    // 提取 MBAP 头
    uint16_t transactionId = (request[0] << 8) | request[1];
    uint16_t protocolId = (request[2] << 8) | request[3];
    uint16_t length = (request[4] << 8) | request[5]; // 后续字节数
    uint8_t unitId = request[6];

    // 只支持 Modbus 协议 (protocolId = 0)
    if (protocolId != 0)
        return;

    // 提取 PDU
    uint8_t functionCode = request[7];

    // 目前只实现功能码 03：读保持寄存器
    if (functionCode == 0x03)
    {
        // 需要至少还有 4 个字节：起始地址(2) + 寄存器数量(2)
        if (len < 11)
            return;

        uint16_t startAddr = (request[8] << 8) | request[9];
        uint16_t quantity = (request[10] << 8) | request[11];

        // 检查数量范围（Modbus 最大允许 125 个寄存器，这里限制为 2）
        if (quantity < 1 || quantity > 2)
        {
            // 返回异常响应：非法数据值 (异常码 0x03)
            uint8_t exceptionRsp[9] = {
                (uint8_t)(transactionId >> 8), (uint8_t)(transactionId & 0xFF),
                (uint8_t)(protocolId >> 8), (uint8_t)(protocolId & 0xFF),
                0x00, 0x03, // 长度 = 3（unitId + functionCode + exceptionCode）
                unitId,
                (uint8_t)(functionCode | 0x80), // 功能码 + 0x80
                0x03                            // 异常码：非法数据值
            };
            Serial.println("返回异常: 非法数据值 (0x03)");
            sendModbusResponse(exceptionRsp, 9);
            return;
        }

        // 检查地址范围（我们只支持地址 0 和 1）
        if (startAddr + quantity > 2)
        {
            // 返回异常响应：非法数据地址 (异常码 0x02)
            uint8_t exceptionRsp[9] = {
                (uint8_t)(transactionId >> 8), (uint8_t)(transactionId & 0xFF),
                (uint8_t)(protocolId >> 8), (uint8_t)(protocolId & 0xFF),
                0x00, 0x03,
                unitId,
                (uint8_t)(functionCode | 0x80),
                0x02};
            Serial.println("返回异常: 非法数据地址 (0x02)");
            sendModbusResponse(exceptionRsp, 9);
            return;
        }

        // 构建正常响应
        int byteCount = quantity * 2;    // 每个寄存器 2 字节
        uint8_t response[9 + byteCount]; // MBAP(7) + PDU(2 + byteCount)

        // MBAP 头
        response[0] = transactionId >> 8;
        response[1] = transactionId & 0xFF;
        response[2] = protocolId >> 8;
        response[3] = protocolId & 0xFF;
        response[4] = (uint8_t)((byteCount + 3) >> 8); // 长度 = unitId(1) + functionCode(1) + byteCount
        response[5] = (uint8_t)((byteCount + 3) & 0xFF);
        response[6] = unitId;

        // PDU: 功能码 + 字节数 + 寄存器数据
        response[7] = functionCode; // 0x03
        response[8] = byteCount;    // 后续字节数

        // 填入寄存器值（大端序：高字节在前）
        for (int i = 0; i < quantity; i++)
        {
            uint16_t regValue = holdingRegisters[startAddr + i];
            response[9 + i * 2] = regValue >> 8;
            response[9 + i * 2 + 1] = regValue & 0xFF;
        }

        Serial.println("构建正常响应，准备发送");
        sendModbusResponse(response, 9 + byteCount);
    }
    else
    {
        // 不支持的功能码 -> 返回异常码 0x01（非法功能）
        uint8_t exceptionRsp[9] = {
            (uint8_t)(transactionId >> 8), (uint8_t)(transactionId & 0xFF),
            (uint8_t)(protocolId >> 8), (uint8_t)(protocolId & 0xFF),
            0x00, 0x03,
            unitId,
            (uint8_t)(functionCode | 0x80),
            0x01};
        Serial.println("返回异常: 非法功能 (0x01)");
        sendModbusResponse(exceptionRsp, 9);
    }
}

// ========== setup ==========
void setup()
{
    Serial.begin(115200);
    Serial.println("启动 ESP32 Modbus TCP 从站（原生实现）");

    dht.begin();

    // 连接 WiFi
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi 连接成功");
    Serial.print("ESP32 IP 地址: ");
    Serial.println(WiFi.localIP());

    // 启动 Modbus TCP 服务器
    modbusServer.begin();
    Serial.println("Modbus TCP 服务器已启动，端口 502");
}

// ========== loop ==========
void loop()
{
    // 1. 每 2 秒更新一次传感器数据并刷新寄存器
    static unsigned long lastSensorRead = 0;
    if (millis() - lastSensorRead >= 2000)
    {
        updateSensorData();
        lastSensorRead = millis();
    }

    // 2. 检查是否有新的 Modbus 客户端连接
    if (!client.connected())
    {
        client = modbusServer.available();
        if (client.connected())
        {
            Serial.println("新 Modbus 客户端已连接");
        }
    }

    // 3. 处理已连接客户端的请求
    if (client.connected() && client.available())
    {
        // 读取 Modbus 请求报文（最多 256 字节）
        uint8_t buffer[256];
        int len = client.read(buffer, sizeof(buffer));
        if (len > 0)
        {
            handleModbusRequest(buffer, len);
        }
    }

    // 短暂延时避免 CPU 空转
    delay(10);
}