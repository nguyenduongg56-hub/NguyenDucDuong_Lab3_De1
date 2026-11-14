/*
 * RUNNER MONITOR - ESP32 + BMP180 + MQTT + FreeRTOS
 *  - TaskRead     (Core 1): doc BMP180, in Serial, day vao dataQueue + alertQueue
 *  - TaskAlert    (Core 1): doc alertQueue, canh bao Led/Coi (AUTO) hoac theo nut Web (MANUAL)
 *  - TaskTransmit (Core 0): ket noi WiFi + MQTT, doc dataQueue, publish; nhan lenh dieu khien
 *
 * Chan: SDA=21, SCL=22, LED=26 (qua R220), BUZZER=25
 */
#include <WiFi.h>
#include <Wire.h>
#include <PubSubClient.h>
#include <Adafruit_BMP085.h>

// ---------- Cau hinh ----------
const char* WIFI_SSID = "Wokwi-GUEST";   // WiFi mo phong cua Wokwi
const char* WIFI_PASS = "";
const char* MQTT_HOST = "broker.hivemq.com";
const int   MQTT_PORT = 1883;

// DOI "runner_demo_01" THANH CHUOI RIENG CUA BAN (broker public, tranh trung nguoi khac)
#define TOPIC_BASE   "runner_demo_01"
#define TOPIC_DATA   TOPIC_BASE "/data"
#define TOPIC_LED    TOPIC_BASE "/cmd/led"
#define TOPIC_BUZ    TOPIC_BASE "/cmd/buzzer"
#define TOPIC_MODE   TOPIC_BASE "/cmd/mode"

#define PIN_LED 26
#define PIN_BUZ 25

const float SEA_LEVEL_PA = 101325.0;  // ap suat chuan muc nuoc bien
const float TEMP_HIGH    = 38.0;      // ngưỡng nhiet do cao (°C)
const float TEMP_LOW     = 5.0;       // ngưỡng nhiet do thap (°C)
const float ALT_JUMP     = 3.0;       // thay doi do cao dot ngot giua 2 lan do (m)
const uint32_t READ_MS   = 1000;      // chu ky doc cam bien

// ---------- Kieu du lieu & hang doi ----------
struct SensorData { float temp; float press; float alt; };   // day du -> data Queue
struct AlertData  { float temp; float alt; };                // canh bao -> data Alert

QueueHandle_t dataQueue;
QueueHandle_t alertQueue;

// Trang thai chia se (ghi boi 1 task, doc boi task khac)
enum AlertCode { ALERT_NONE = 0, ALERT_TEMP_HIGH, ALERT_TEMP_LOW, ALERT_ALT_JUMP };
volatile int  alertCode   = ALERT_NONE;
volatile bool manualMode  = false;
volatile bool manualLed   = false;
volatile bool manualBuz   = false;
volatile bool ledOut      = false;
volatile bool buzOut      = false;

Adafruit_BMP085 bmp;
WiFiClient espClient;
PubSubClient mqtt(espClient);

// ---------- Nhan lenh tu Web (chay trong TaskTransmit) ----------
void mqttCallback(char* topic, byte* payload, unsigned int len) {
  String msg;
  for (unsigned int i = 0; i < len; i++) msg += (char)payload[i];
  msg.trim(); msg.toUpperCase();
  bool on = (msg == "ON" || msg == "1");
  String t = String(topic);
  if (t == TOPIC_LED)       manualLed  = on;
  else if (t == TOPIC_BUZ)  manualBuz  = on;
  else if (t == TOPIC_MODE) manualMode = (msg == "MANUAL");
  Serial.printf("[CMD] %s = %s\n", topic, msg.c_str());
}

// ---------- TASK DOC DU LIEU (Core 1) ----------
void taskRead(void* pv) {
  float prevAlt = NAN;
  TickType_t last = xTaskGetTickCount();
  for (;;) {
    SensorData d;
    d.temp  = bmp.readTemperature();
    d.press = bmp.readPressure() / 100.0;              // hPa
    d.alt   = bmp.readAltitude(SEA_LEVEL_PA);          // m so voi muc nuoc bien

    Serial.printf("Temp: %.1f C | Pressure: %.1f hPa | Altitude: %.1f m\n",
                  d.temp, d.press, d.alt);

    // Du lieu day du -> data Queue (khong chan neu day, bo ban tin cu nhat)
    if (xQueueSend(dataQueue, &d, 0) != pdTRUE) {
      SensorData tmp; xQueueReceive(dataQueue, &tmp, 0);
      xQueueSend(dataQueue, &d, 0);
    }

    // Du lieu canh bao (nhiet do + do cao) -> data Alert (chi giu ban moi nhat)
    AlertData a = { d.temp, d.alt };
    xQueueOverwrite(alertQueue, &a);

    vTaskDelayUntil(&last, pdMS_TO_TICKS(READ_MS));
  }
}

// ---------- TASK CANH BAO (Core 1) ----------
void taskAlert(void* pv) {
  float prevAlt = NAN;
  int   code = ALERT_NONE;
  uint32_t jumpUntil = 0;     // giu canh bao do cao thay doi trong 5 s
  bool blink = false;

  for (;;) {
    AlertData a;
    if (xQueueReceive(alertQueue, &a, pdMS_TO_TICKS(200)) == pdTRUE) {
      code = ALERT_NONE;
      if (a.temp > TEMP_HIGH)      code = ALERT_TEMP_HIGH;
      else if (a.temp < TEMP_LOW)  code = ALERT_TEMP_LOW;

      if (!isnan(prevAlt) && fabs(a.alt - prevAlt) > ALT_JUMP)
        jumpUntil = millis() + 5000;
      prevAlt = a.alt;

      if (code == ALERT_NONE && millis() < jumpUntil) code = ALERT_ALT_JUMP;
      alertCode = code;
    }
    if (code == ALERT_ALT_JUMP && millis() >= jumpUntil) { code = ALERT_NONE; alertCode = code; }

    blink = !blink;   // nhap nhay moi 200 ms
    bool led, buz;
    if (manualMode) {                 // MANUAL: theo nut tren Web
      led = manualLed;
      buz = manualBuz;
    } else {                          // AUTO: theo canh bao
      led = (code != ALERT_NONE) && blink;
      buz = (code != ALERT_NONE) && blink;
    }
    ledOut = led; buzOut = buz;
    digitalWrite(PIN_LED, led);
    if (buz) tone(PIN_BUZ, 2000); else noTone(PIN_BUZ);
  }
}

// ---------- TASK TRUYEN DU LIEU (Core 0) ----------
const char* alertText(int c) {
  switch (c) {
    case ALERT_TEMP_HIGH: return "TEMP_HIGH";
    case ALERT_TEMP_LOW:  return "TEMP_LOW";
    case ALERT_ALT_JUMP:  return "ALT_JUMP";
    default:              return "OK";
  }
}

void taskTransmit(void* pv) {
  for (;;) {
    // 1) WiFi
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("[WiFi] Connecting...");
      WiFi.begin(WIFI_SSID, WIFI_PASS);
      uint32_t t0 = millis();
      while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) vTaskDelay(pdMS_TO_TICKS(250));
      if (WiFi.status() != WL_CONNECTED) { vTaskDelay(pdMS_TO_TICKS(2000)); continue; }
      Serial.print("[WiFi] OK, IP: "); Serial.println(WiFi.localIP());
    }
    // 2) MQTT
    if (!mqtt.connected()) {
      String cid = "runner-esp32-" + String((uint32_t)ESP.getEfuseMac(), HEX);
      if (mqtt.connect(cid.c_str())) {
        mqtt.subscribe(TOPIC_LED);
        mqtt.subscribe(TOPIC_BUZ);
        mqtt.subscribe(TOPIC_MODE);
        Serial.println("[MQTT] connected");
      } else {
        vTaskDelay(pdMS_TO_TICKS(2000));
        continue;
      }
    }
    // 3) Doc data Queue va publish
    SensorData d;
    if (xQueueReceive(dataQueue, &d, pdMS_TO_TICKS(100)) == pdTRUE) {
      char buf[200];
      snprintf(buf, sizeof(buf),
        "{\"temp\":%.1f,\"press\":%.1f,\"alt\":%.1f,\"alert\":\"%s\",\"led\":%d,\"buz\":%d,\"mode\":\"%s\"}",
        d.temp, d.press, d.alt, alertText(alertCode), ledOut ? 1 : 0, buzOut ? 1 : 0,
        manualMode ? "MANUAL" : "AUTO");
      mqtt.publish(TOPIC_DATA, buf);
    }
    mqtt.loop();
  }
}

// ---------- SETUP ----------
void setup() {
  Serial.begin(115200);
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_BUZ, OUTPUT);

  Wire.begin(21, 22);
  if (!bmp.begin()) {
    Serial.println("Khong tim thay BMP180!");
    while (true) delay(1000);
  }

  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(mqttCallback);

  dataQueue  = xQueueCreate(10, sizeof(SensorData));
  alertQueue = xQueueCreate(1,  sizeof(AlertData));

  xTaskCreatePinnedToCore(taskTransmit, "TaskTransmit", 8192, NULL, 1, NULL, 0);  // Core 0
  xTaskCreatePinnedToCore(taskRead,     "TaskRead",     4096, NULL, 2, NULL, 1);  // Core 1
  xTaskCreatePinnedToCore(taskAlert,    "TaskAlert",    4096, NULL, 3, NULL, 1);  // Core 1
}

void loop() { vTaskDelete(NULL); }   // moi viec do cac task dam nhiem
