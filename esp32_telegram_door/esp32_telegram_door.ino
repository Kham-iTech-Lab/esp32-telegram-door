/*
  ESP32 แจ้งเตือนประตูเปิดเข้า Telegram
  โค้ดประกอบคลิป K iTech Review / Kham iTech Lab

  ต่อวงจร: reed switch (เซนเซอร์ประตูแม่เหล็ก) 2 สาย
    สายหนึ่ง -> GPIO 4
    อีกสาย   -> GND
  ใช้ pull-up ภายในบอร์ด ไม่ต้องมีตัวต้านทานเพิ่ม
    แม่เหล็กอยู่ใกล้ (ประตูปิด) = LOW
    แม่เหล็กห่าง   (ประตูเปิด) = HIGH
*/
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "secrets.h"   // คัดลอกจาก secrets.example.h แล้วแก้ค่า

// ---------- ตั้งค่า ----------
const int  DOOR_PIN        = 4;      // ขาที่ต่อ reed switch
const bool NOTIFY_ON_CLOSE = false;  // true = แจ้งตอนประตูปิดด้วย
const unsigned long DEBOUNCE_MS = 80;    // กันสัญญาณเด้ง
const unsigned long COOLDOWN_MS = 5000;  // เว้นระยะแจ้งเตือนขั้นต่ำ กันข้อความรัว

// ---------- ตัวแปร ----------
int lastStable = -1;
int lastRead = -1;
unsigned long lastChange = 0;
unsigned long lastNotify = 0;

String urlEncode(const String &s) {
  String out;
  const char *hex = "0123456789ABCDEF";
  for (size_t i = 0; i < s.length(); i++) {
    uint8_t c = (uint8_t)s[i];
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      out += (char)c;
    } else {
      out += '%';
      out += hex[c >> 4];
      out += hex[c & 0x0F];
    }
  }
  return out;
}

bool sendTelegram(const String &text) {
  if (WiFi.status() != WL_CONNECTED) return false;
  WiFiClientSecure client;
  client.setInsecure();  // ง่ายสำหรับโปรเจกต์บ้าน (ไม่ตรวจใบรับรอง)
  HTTPClient http;
  String url = String("https://api.telegram.org/bot") + BOT_TOKEN + "/sendMessage";
  if (!http.begin(client, url)) return false;
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  String body = String("chat_id=") + CHAT_ID + "&text=" + urlEncode(text);
  int code = http.POST(body);
  Serial.printf("Telegram HTTP %d\n", code);
  if (code != 200) Serial.println(http.getString());
  http.end();
  return code == 200;
}

void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  Serial.printf("Connecting to %s", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 20000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(WiFi.status() == WL_CONNECTED ? "\nWiFi OK: " + WiFi.localIP().toString() : "\nWiFi failed, retry later");
}

void setup() {
  Serial.begin(115200);
  pinMode(DOOR_PIN, INPUT_PULLUP);
  connectWiFi();
  lastStable = lastRead = digitalRead(DOOR_PIN);
  sendTelegram(String("✅ ระบบแจ้งเตือนประตูพร้อมแล้ว\nตอนนี้ประตู") + (lastStable == HIGH ? "เปิดอยู่" : "ปิดอยู่"));
}

void loop() {
  static unsigned long lastWiFiCheck = 0;
  if (millis() - lastWiFiCheck > 10000) {   // เช็ก Wi-Fi ทุก 10 วินาที
    lastWiFiCheck = millis();
    connectWiFi();
  }

  int r = digitalRead(DOOR_PIN);
  if (r != lastRead) {
    lastRead = r;
    lastChange = millis();
  }
  if (millis() - lastChange > DEBOUNCE_MS && r != lastStable) {
    lastStable = r;
    bool opened = (r == HIGH);
    Serial.println(opened ? "Door OPEN" : "Door CLOSED");
    if (millis() - lastNotify > COOLDOWN_MS || lastNotify == 0) {
      if (opened) {
        sendTelegram("🚪 ประตูเปิด!");
        lastNotify = millis();
      } else if (NOTIFY_ON_CLOSE) {
        sendTelegram("🔒 ประตูปิดแล้ว");
        lastNotify = millis();
      }
    }
  }
  delay(10);
}
