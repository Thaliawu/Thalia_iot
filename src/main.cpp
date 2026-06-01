#include <Arduino.h>
#include <WiFi.h>
#include "DHT.h"
#include <esp_task_wdt.h>

// ========== 硬件配置 ==========
#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// 仅保留绿灯（ESP32 板载 LED 通常为 GPIO2，高电平亮）
#define LED_GREEN 2

// ========== WiFi 配置 ==========
const char *ssid = "Thalia";        // 你的 WiFi 名称
const char *password = "ssjw12138"; // 你的 WiFi 密码

// ========== Modbus TCP 配置 ==========
WiFiServer modbusServer(502);
WiFiClient client;
uint16_t holdingRegisters[2] = {0, 0}; // 温度*10, 湿度*10

// ========== EEPROM 配置 ==========
#include <EEPROM.h>
#define EEPROM_SIZE 64
#define EEPROM_ADDR_TEMP 0
#define EEPROM_ADDR_HUM 2
#define EEPROM_ADDR_CRC 4

// CRC16 函数（用于校验 EEPROM 数据）
uint16_t crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++)
    {
        crc ^= data[i];
        for (int j = 0; j < 8; j++)
        {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xA001;
            else
                crc >>= 1;
        }
    }
    return crc;
}

// 保存寄存器值到 EEPROM（带 CRC）
void saveToEEPROM()
{
    uint16_t temp = holdingRegisters[0];
    uint16_t hum = holdingRegisters[1];
    EEPROM.put(EEPROM_ADDR_TEMP, temp);
    EEPROM.put(EEPROM_ADDR_HUM, hum);
    uint16_t crc = crc16((uint8_t *)&temp, sizeof(temp)) ^ crc16((uint8_t *)&hum, sizeof(hum));
    EEPROM.put(EEPROM_ADDR_CRC, crc);
    EEPROM.commit();
    Serial.println("EEPROM 已保存");
}

// 从 EEPROM 恢复寄存器值（检查 CRC）
void loadFromEEPROM()
{
    uint16_t temp, hum, storedCrc;
    EEPROM.get(EEPROM_ADDR_TEMP, temp);
    EEPROM.get(EEPROM_ADDR_HUM, hum);
    EEPROM.get(EEPROM_ADDR_CRC, storedCrc);
    uint16_t calcCrc = crc16((uint8_t *)&temp, sizeof(temp)) ^ crc16((uint8_t *)&hum, sizeof(hum));
    if (storedCrc == calcCrc && calcCrc != 0)
    {
        holdingRegisters[0] = temp;
        holdingRegisters[1] = hum;
        Serial.printf("EEPROM 恢复: 温度=%d, 湿度=%d\n", temp, hum);
    }
    else
    {
        Serial.println("EEPROM 数据无效，使用默认值");
        holdingRegisters[0] = 250; // 25.0°C
        holdingRegisters[1] = 600; // 60.0%
    }
}

// ========== 环形缓冲区（存储最近 N 条传感器数据）==========
#define RING_BUFFER_SIZE 10
struct SensorRecord
{
    uint16_t temp;
    uint16_t hum;
    uint32_t timestamp;
};
SensorRecord ringBuffer[RING_BUFFER_SIZE];
int ringHead = 0;
int ringCount = 0;

void addToRingBuffer(uint16_t temp, uint16_t hum)
{
    ringBuffer[ringHead].temp = temp;
    ringBuffer[ringHead].hum = hum;
    ringBuffer[ringHead].timestamp = millis();
    ringHead = (ringHead + 1) % RING_BUFFER_SIZE;
    if (ringCount < RING_BUFFER_SIZE)
        ringCount++;
}

// ========== 更新传感器数据 ==========
void updateSensorData()
{
    static unsigned long lastRead = 0;
    if (millis() - lastRead < 2000)
        return;
    lastRead = millis();

    float temp = dht.readTemperature();
    float hum = dht.readHumidity();
    if (!isnan(temp) && !isnan(hum))
    {
        uint16_t newTemp = (uint16_t)(temp * 10);
        uint16_t newHum = (uint16_t)(hum * 10);
        holdingRegisters[0] = newTemp;
        holdingRegisters[1] = newHum;
        addToRingBuffer(newTemp, newHum);
        Serial.printf("温度: %.1f℃ (reg=%d)  湿度: %.1f%% (reg=%d)\n", temp, newTemp, hum, newHum);
    }
    else
    {
        Serial.println("DHT11 读取失败");
    }
}

// ========== WiFi 断线重连（指数退避）==========
void checkWiFi()
{
    static uint8_t retryCount = 0;
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("WiFi 断开，尝试重连...");
        WiFi.disconnect();
        WiFi.begin(ssid, password);
        int delayMs = 500 * (1 << retryCount);
        if (delayMs > 10000)
            delayMs = 10000;
        delay(delayMs);
        retryCount++;
        if (retryCount > 5)
            retryCount = 5;
    }
    else
    {
        retryCount = 0;
    }
}

// ========== 看门狗配置（30 秒）==========
void setupWatchdog()
{
    esp_task_wdt_init(30, true);
    esp_task_wdt_add(NULL);
}

void feedWatchdog()
{
    esp_task_wdt_reset();
}

// ========== 简单 LED 指示（绿灯闪烁表示运行正常）==========
void updateLED()
{
    static unsigned long lastToggle = 0;
    // 每 2 秒闪烁一次，表示系统运行中
    if (millis() - lastToggle >= 2000)
    {
        lastToggle = millis();
        digitalWrite(LED_GREEN, !digitalRead(LED_GREEN));
    }
}

// ========== Modbus TCP 处理函数 ==========
void sendModbusResponse(uint8_t *response, int len)
{
    if (client.connected())
    {
        client.write(response, len);
        client.flush();
    }
}

void handleModbusRequest(uint8_t *request, int len)
{
    if (len < 9)
        return;
    uint16_t transactionId = (request[0] << 8) | request[1];
    uint16_t protocolId = (request[2] << 8) | request[3];
    uint8_t unitId = request[6];
    if (protocolId != 0)
        return;
    uint8_t functionCode = request[7];
    if (functionCode == 0x03)
    {
        if (len < 11)
            return;
        uint16_t startAddr = (request[8] << 8) | request[9];
        uint16_t quantity = (request[10] << 8) | request[11];
        if (quantity < 1 || quantity > 2)
        {
            uint8_t exceptionRsp[9] = {
                (uint8_t)(transactionId >> 8), (uint8_t)(transactionId & 0xFF),
                (uint8_t)(protocolId >> 8), (uint8_t)(protocolId & 0xFF),
                0x00, 0x03, unitId,
                (uint8_t)(functionCode | 0x80), 0x03};
            sendModbusResponse(exceptionRsp, 9);
            return;
        }
        if (startAddr + quantity > 2)
        {
            uint8_t exceptionRsp[9] = {
                (uint8_t)(transactionId >> 8), (uint8_t)(transactionId & 0xFF),
                (uint8_t)(protocolId >> 8), (uint8_t)(protocolId & 0xFF),
                0x00, 0x03, unitId,
                (uint8_t)(functionCode | 0x80), 0x02};
            sendModbusResponse(exceptionRsp, 9);
            return;
        }
        int byteCount = quantity * 2;
        uint8_t response[9 + byteCount];
        response[0] = transactionId >> 8;
        response[1] = transactionId & 0xFF;
        response[2] = protocolId >> 8;
        response[3] = protocolId & 0xFF;
        response[4] = (byteCount + 3) >> 8;
        response[5] = (byteCount + 3) & 0xFF;
        response[6] = unitId;
        response[7] = functionCode;
        response[8] = byteCount;
        for (int i = 0; i < quantity; i++)
        {
            uint16_t val = holdingRegisters[startAddr + i];
            response[9 + i * 2] = val >> 8;
            response[9 + i * 2 + 1] = val & 0xFF;
        }
        sendModbusResponse(response, 9 + byteCount);
    }
    else
    {
        uint8_t exceptionRsp[9] = {
            (uint8_t)(transactionId >> 8), (uint8_t)(transactionId & 0xFF),
            (uint8_t)(protocolId >> 8), (uint8_t)(protocolId & 0xFF),
            0x00, 0x03, unitId,
            (uint8_t)(functionCode | 0x80), 0x01};
        sendModbusResponse(exceptionRsp, 9);
    }
}

// ========== setup ==========
void setup()
{
    Serial.begin(115200);
    Serial.println("启动 ESP32 工业级从站（可靠性增强版）");

    // 初始化 LED
    pinMode(LED_GREEN, OUTPUT);
    digitalWrite(LED_GREEN, LOW);

    // 初始化 EEPROM
    EEPROM.begin(EEPROM_SIZE);
    loadFromEEPROM();

    // 初始化 DHT11
    dht.begin();

    // 连接 WiFi
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    Serial.print("连接 WiFi");
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi 已连接，IP: " + WiFi.localIP().toString());

    // 启动 Modbus 服务器
    modbusServer.begin();
    Serial.println("Modbus TCP 服务器启动，端口 502");

    // 启动看门狗
    setupWatchdog();
    Serial.println("看门狗已启用（30秒）");
}

// ========== loop ==========
void loop()
{
    feedWatchdog();

    updateSensorData(); // 每2秒更新
    checkWiFi();        // 检测并重连
    updateLED();        // 绿灯闪烁表示运行中

    // 每30秒保存一次寄存器到EEPROM
    static unsigned long lastSave = 0;
    if (millis() - lastSave >= 30000)
    {
        saveToEEPROM();
        lastSave = millis();
    }

    // Modbus 客户端管理
    if (!client.connected())
    {
        client = modbusServer.available();
        if (client.connected())
        {
            Serial.println("新 Modbus 客户端连接");
        }
    }

    if (client.connected() && client.available())
    {
        uint8_t buffer[256];
        int len = client.read(buffer, sizeof(buffer));
        if (len > 0)
        {
            handleModbusRequest(buffer, len);
        }
    }

    delay(10);
}