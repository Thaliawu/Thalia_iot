#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <esp_task_wdt.h>

/* ========== 配置区 ========== */
#define WIFI_SSID "Thalia"
#define WIFI_PASS "ssjw12138"

#define MQTT_SERVER "broker.emqx.io" // 免费公共 MQTT
#define MQTT_PORT 1883
#define MQTT_TOPIC "thalia/iot/temp_humi"

#define DHTPIN 4
#define DHTTYPE DHT11
/* ============================= */

DHT dht(DHTPIN, DHTTYPE);
WiFiClient espClient;
PubSubClient client(espClient);

/* ========== WiFi ========== */
void wifi_connect()
{
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.print("WiFi Connecting");

    int retry = 0;
    while (WiFi.status() != WL_CONNECTED && retry < 20)
    {
        delay(500);
        Serial.print(".");
        retry++;
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println("\nWiFi Connected");
    }
    else
    {
        Serial.println("\nWiFi Failed");
    }
}

/* ========== MQTT ========== */
void mqtt_reconnect()
{
    while (!client.connected())
    {
        Serial.print("MQTT Connecting...");
        if (client.connect("ESP32_Thalia"))
        {
            Serial.println("connected");
        }
        else
        {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            delay(2000);
        }
    }
}

/* ========== Setup ========== */
void setup()
{
    Serial.begin(115200);
    delay(1000);

    Serial.println("System Boot");

    dht.begin();
    wifi_connect();

    client.setServer(MQTT_SERVER, MQTT_PORT);

    /* 软件看门狗：10 秒不喂狗重启 */
    esp_task_wdt_init(10, true);
    esp_task_wdt_add(NULL);
}

/* ========== Loop ========== */
void loop()
{
    /* WiFi 断线重连 */
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("WiFi Lost, Reconnect...");
        wifi_connect();
    }

    /* MQTT 断线重连 */
    if (!client.connected())
    {
        mqtt_reconnect();
    }
    client.loop();

    /* 读取传感器 */
    float temp = dht.readTemperature();
    float humi = dht.readHumidity();

    if (isnan(temp) || isnan(humi))
    {
        Serial.println("[ERR] DHT read failed");
    }
    else
    {
        char payload[128];
        snprintf(payload, sizeof(payload),
                 "{\"temp\":%.1f,\"humi\":%.1f}", temp, humi);

        Serial.printf("Publish: %s\n", payload);
        client.publish(MQTT_TOPIC, payload);
    }

    /* 喂狗 */
    esp_task_wdt_reset();

    delay(5000);
}