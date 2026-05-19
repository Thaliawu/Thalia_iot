#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <DHT.h>   // 使用你刚安装的 Adafruit 库
#include <DHT_U.h> // Adafruit 统一传感器接口
#include <EEPROM.h>

// CRC16 校验函数 (Modbus 标准)
uint16_t calculateCRC16(uint8_t *data, uint16_t length)
{
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < length; i++)
    {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++)
        {
            if ((crc & 0x0001) != 0)
            {
                crc >>= 1;
                crc ^= 0xA001;
            }
            else
            {
                crc >>= 1;
            }
        }
    }
    return crc;
}

// =========================
// 1. 引脚与对象定义
// =========================
#define DHT_PIN 4
#define DHT_TYPE DHT11 // 明确指定是 DHT11
#define EEPROM_SIZE 64

DHT dht(DHT_PIN, DHT_TYPE); // 创建 DHT 传感器对象

// 模拟 Modbus 保持寄存器 (Holding Registers)
// 地址 0: 温度 (x10), 地址 1: 湿度 (x10)
volatile uint16_t modbus_regs[2] = {0, 0};

QueueHandle_t sensor_queue;

// ✅ 新增：用于 EEPROM 定时保存的时间戳
static uint32_t lastSaveTime = 0;
// =========================
// 2. 采样任务（高优先级）
// =========================
void sampling_task(void *pv)
{
    dht.begin(); // 初始化传感器

    float temp, humi;

    while (1)
    {
        temp = dht.readTemperature(); // 读取温度
        humi = dht.readHumidity();    // 读取湿度

        // 检查读数是否合法
        if (!isnan(temp) && !isnan(humi))
        {
            // 把数据放入队列
            float data[2] = {temp, humi};
            xQueueSend(sensor_queue, data, portMAX_DELAY);
        }
        vTaskDelay(pdMS_TO_TICKS(2000)); // 2秒采集一次
    }
}

// =========================
// 3. 通信任务（低优先级）
// =========================
void comm_task(void *pv)
{
    float data[2];

    while (1)
    {
        if (xQueueReceive(sensor_queue, data, portMAX_DELAY))
        {
            // 简历亮点：将数据写入 Modbus 寄存器
            // 工业标准：浮点数转整数（放大10倍，保留一位小数）
            // 简历亮点：将数据写入 Modbus 寄存器
            // 工业标准：浮点数转整数（放大10倍，保留一位小数）
            modbus_regs[0] = (uint16_t)(data[0] * 10); // 温度
            modbus_regs[1] = (uint16_t)(data[1] * 10); // 湿度

            Serial.print("Modbus Regs -> Temp: ");
            Serial.print(modbus_regs[0]);
            Serial.print(", Humi: ");
            Serial.println(modbus_regs[1]);

            // ============================================================
            // ✅ 这里开始：插入你刚才准备好的 CRC 校验代码块
            // ============================================================
            // 1. 准备发送的数据包
            uint8_t response[6]; // ⚠️ 注意：这里要改成 6，因为有4个数据位+2个CRC位
            response[0] = highByte(modbus_regs[0]);
            response[1] = lowByte(modbus_regs[0]);
            response[2] = highByte(modbus_regs[1]);
            response[3] = lowByte(modbus_regs[1]);

            // 2. 计算 CRC
            uint16_t crc = calculateCRC16(response, 4);

            // 3. 把 CRC 追加到数据包后面
            response[4] = lowByte(crc);  // CRC 低字节
            response[5] = highByte(crc); // CRC 高字节

            // 4. 发送出去
            Serial.write(response, 6);
            // ============================================================
            // ✅ 这里结束：CRC 校验代码块
            // ============================================================

            // ✅ 新增：用于 EEPROM 定时保存的时间戳
            // 检查读数是否合法 (其实这里主要是定时器逻辑)
            // 注意：原来的代码里这里有个 if (!isnan...) 判断，但在 comm_task 里 data 已经是 float 了
            // 我们直接保留你的 EEPROM 逻辑，但把它放在 CRC 发送之后

            // 每 30 秒保存一次
            if (millis() - lastSaveTime > 30000)
            {
                EEPROM.write(0, modbus_regs[0]);
                EEPROM.write(1, modbus_regs[1]);
                EEPROM.commit();
                Serial.println("EEPROM Saved.");
                lastSaveTime = millis();
            }

            // ✅ EEPROM 定时保存（每 30 秒写一次）
        }
    }
}

// =========================
// 4. 初始化
// =========================
// =========================
// 4. 初始化
// =========================
void setup()
{
    Serial.begin(115200);
    delay(1000);

    // 1. EEPROM 初始化
    EEPROM.begin(EEPROM_SIZE);

    // 2. ✅ 先创建队列（必须先有碗，才能盛饭）
    sensor_queue = xQueueCreate(5, sizeof(float) * 2);

    // 3. 从 EEPROM 恢复数据
    uint16_t saved_temp = EEPROM.read(0);
    uint16_t saved_humi = EEPROM.read(1);

    Serial.print("开机恢复 -> Temp: ");
    Serial.print(saved_temp);
    Serial.print(", Humi: ");
    Serial.println(saved_humi);

    // 4. 恢复寄存器
    modbus_regs[0] = saved_temp;
    modbus_regs[1] = saved_humi;

    // 5. ✅ 把恢复的数据送入队列（队列已存在，安全）
    // 注意：这里加上 (float) 或者 10.0f 是为了消除编译器的 narrowing warning
    float restored_data[2] = {
        (float)saved_temp / 10.0f,
        (float)saved_humi / 10.0f};
    xQueueSend(sensor_queue, restored_data, portMAX_DELAY);

    // 6. 创建任务
    xTaskCreate(sampling_task, "Sampling", 2048, NULL, 3, NULL);
    xTaskCreate(comm_task, "Comm", 2048, NULL, 2, NULL);

    vTaskDelete(NULL);
}

void loop()
{
    // 留空，由 FreeRTOS 接管
}