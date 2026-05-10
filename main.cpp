#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <ir_Mitsubishi.h>
#include <time.h>

// ---- WiFi設定 ----
const char* ssid = "ssid";         // Wi-Fi SSID
const char* password = "pas"; // Wi-Fi パスワード

WebServer server(80); // ポート8080でWebサーバを起動
const uint16_t kIrLed = 4; // 赤外線LED接続ピン
IRMitsubishiAC ac(kIrLed);
IRsend irsend(kIrLed);  // acとは別に生データ用送信機も使う

bool acPowerOn = false; // エアコンの電源状態
bool fanPowerOn = false;  // サーキュレーター電源状態
bool isHeating = false;  // false = 冷房, true = 暖房
String lastLog = "まだ操作されていません";

// ---- HTMLコンテンツ (フラッシュメモリに配置) ----
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="ja">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>スマートリモコン</title>
  <style>
    @import url(http://fonts.googleapis.com/earlyaccess/notosansjp.css);
    :root {
      --main-bg: #f0f0f5;
      --btn-grad: linear-gradient(to right, #36d1dc, #5b86e5);
      --btn-hover: linear-gradient(to right, #5b86e5, #36d1dc);
    }

    body {
      font-family: 'Noto Sans JP', sans-serif;
      margin: 0;
      padding: 0;
      background: var(--main-bg);
      color: #333;
      display: flex;
      justify-content: center;
      align-items: center;
      height: 100vh;
    }

    .container {
      max-width: 400px;
      width: 90%;
      background: #fff;
      padding: 2em;
      border-radius: 1em;
      box-shadow: 0 0 20px rgba(0, 0, 0, 0.1);
      text-align: center;
    }

    h2 {
      margin-bottom: 1em;
    }

    input[type="range"] {
      width: 100%;
      margin: 1em 0;
    }

    .temp-display {
      font-size: 1.5em;
      font-weight: bold;
      margin-bottom: 1em;
    }

    button {
      background: var(--btn-grad);
      border: none;
      color: white;
      padding: 0.8em 2em;
      font-size: 1.2em;
      border-radius: 0.5em;
      cursor: pointer;
      transition: all 0.3s ease;
      margin: 1em;
    }

    button:hover {
      background: var(--btn-hover);
      transform: scale(1.05);
    }
  </style>
  </head>
    <body>
      <div class="container">
        <h2>スマートリモコン</h2>
        <div>エアコン</div>
        <div id="powerStatus" style="margin-bottom: 1em; font-weight: bold;">状態: 不明</div>
        <div class="temp-display">
          <span id="tempVal">26</span> ℃
        </div>
        <input type="range" id="temp" name="temp" min="16" max="30" step="0.5" value="26"
             oninput="document.getElementById('tempVal').innerText = parseFloat(this.value).toFixed(1);">
        <button id="modeBtn" onclick="toggleMode()">冷房/暖房 切替</button>
        <div id="modeStatus" style="margin: 1em 0; font-weight: bold;">モード: 取得中...</div>
        <button onclick="sendTemp()">送信</button></br>
        <button onclick="sendStop()">停止</button>
        <div id="fanStatus" style="margin-bottom: 1em; font-weight: bold;">サーキュレーター: 不明</div>
        <button onclick="toggleFan()">サーキュレーターON/OFF</button>
        <div id="lastLog" style="margin-top: 2em; font-size: 0.9em; color: #666;">最終操作: 取得中...</div>
      </div>

    <script>
      function updateLastLog() {
        fetch("/lastlog")
          .then(res => res.text())
          .then(txt => {
            document.getElementById("lastLog").innerText = "最終操作: " + txt;
          });
      }

      function sendTemp() {
        const temp = parseFloat(document.getElementById("temp").value).toFixed(1);
        fetch(`/set?temp=${temp}`)
          .then(res => res.text())
          .then(txt => {
            alert(txt);
            updateStatus();  // 状態更新
            updateLastLog();
          });
      }

      function sendStop() {
        fetch(`/off`)
          .then(res => res.text())
          .then(txt => {
            alert(txt);
            updateStatus();  // 状態更新
            updateLastLog();
          });
      }

      function updateStatus() {
        fetch("/status")
          .then(res => res.json())
          .then(data => {
            document.getElementById("powerStatus").innerText = "状態: " + (data.power ? "たぶんON" : "たぶんOFF");
          });
      }

      function toggleFan() {
        fetch(`/fan_toggle`)
          .then(res => res.text())
          .then(txt => {
            alert(txt);
            updateFanStatus();
            updateLastLog();
          });
      }

      function updateFanStatus() {
        fetch("/fan_status")
          .then(res => res.json())
          .then(data => {
            document.getElementById("fanStatus").innerText =
              "サーキュレーター: " + (data.fan ? "ON" : "OFF");
          });
      }
      function updateMode() {
        fetch("/mode_status")
          .then(res => res.json())
          .then(data => {
            const heating = data.heating;

            document.getElementById("modeStatus").innerText =
              heating ? "モード: 暖房" : "モード: 冷房";

            // UI変更
            const root = document.documentElement;
            if (heating) {
              root.style.setProperty("--main-bg", "#fff5e6");
              root.style.setProperty("--btn-grad", "linear-gradient(to right, #ff8c42, #ff3d2e)");
              root.style.setProperty("--btn-hover", "linear-gradient(to right, #ff3d2e, #ff8c42)");
            } else {
              root.style.setProperty("--main-bg", "#f0f0f5");
              root.style.setProperty("--btn-grad", "linear-gradient(to right, #36d1dc, #5b86e5)");
              root.style.setProperty("--btn-hover", "linear-gradient(to right, #5b86e5, #36d1dc)");
            }
          });
      }

      function toggleMode() {
        fetch("/mode_toggle")
          .then(res => res.text())
          .then(txt => {
            alert(txt);
            updateMode();
            updateLastLog();
          });
      }


      // ページロード時に状態取得
      window.onload = function () {
        updateStatus();      // エアコン
        updateFanStatus();   // サーキュレーター
        updateLastLog();
        updateMode();
      };
    </script>
  </body>
  </html>
)rawliteral";

void handleRoot() {
  server.send_P(200, "text/html", INDEX_HTML);
}

String getCurrentTime() {
  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);
  char buffer[32];
  sprintf(buffer, "%04d-%02d-%02d %02d:%02d:%02d",
          timeinfo->tm_year + 1900,
          timeinfo->tm_mon + 1,
          timeinfo->tm_mday,
          timeinfo->tm_hour,
          timeinfo->tm_min,
          timeinfo->tm_sec);
  return String(buffer);
}

void logAction(String message) {
  lastLog = "[" + getCurrentTime() + "] " + message;
}

void handleLastLog() {
  server.send(200, "text/plain", lastLog);
}

void handleSet() {
  if (server.hasArg("temp")) {
    float temp = server.arg("temp").toFloat();

    if (temp >= 16 && temp <= 30) {
      ac.begin();
      ac.on();
      ac.setMode(kMitsubishiAcCool);
      ac.setTemp(temp); 
      ac.setFan(kMitsubishiAcFanAuto);
      ac.setVane(kMitsubishiAcVaneAuto);
      ac.setWideVane(kMitsubishiAcWideVaneAuto);
      ac.setMode(isHeating ? kMitsubishiAcHeat : kMitsubishiAcCool);
      ac.send();
      acPowerOn = true;
      logAction("エアコン温度を " + String(temp, 1) + "℃ に設定");

      server.send(200, "text/html", "設定温度 "+ String(temp, 1)+ "℃ を送信しました。");
      return;
    }
  }
  
  server.send(400, "text/plain", R"rawliteral(<html><head><meta charset="UTF-8"></head>温度が不正です</html>)rawliteral");
}


void handleOff() {
  ac.begin();
  ac.off();
  ac.send();
  acPowerOn = false;
  logAction("エアコンを停止");

  server.send(200, "text/html", "電源OFFを送信しました。");
}

void handleFanToggle() {
  uint16_t rawData[] = {
    8804, 4650, 382, 754, 384, 728, 410, 702,
    436, 726, 412, 726, 410, 726, 412, 726,
    410, 1848, 410, 728, 410, 1848, 410, 1848,
    412, 1850, 410, 1848, 410, 754, 384, 1850,
    408, 1876, 384, 596, 542, 754, 384, 730,
    408, 752, 384, 754, 382, 780, 358, 754,
    382, 756, 382, 1878, 380, 1878, 380, 1874,
    384, 1848, 412, 1794, 464, 1778, 480, 1792,
    466, 1792, 468
  };

  irsend.sendRaw(rawData, 67, 38);  // トグル信号送信
  fanPowerOn = !fanPowerOn;  // 状態を反転
  logAction("サーキュレーターを " + String(fanPowerOn ? "ON" : "OFF") + " に切り替え");

  server.send(200, "text/plain", fanPowerOn ? "サーキュレーター: たぶんON" : "サーキュレーター: たぶんOFF");
}

void handleStatus() {
  String json = "{\"power\": ";
  json += (acPowerOn ? "true" : "false");
  json += "}";
  server.send(200, "application/json", json);
}

void handleFanStatus() {
  String json = "{\"fan\": ";
  json += (fanPowerOn ? "true" : "false");
  json += "}";
  server.send(200, "application/json", json);
}

void handleModeToggle() {
  isHeating = !isHeating;

  logAction(isHeating ? "モードを暖房に切り替え" : "モードを冷房に切り替え");

  server.send(200, "text/plain", isHeating ? "暖房モードに変更しました" : "冷房モードに変更しました");
}

void handleModeStatus() {
  String json = "{\"heating\": ";
  json += (isHeating ? "true" : "false");
  json += "}";
  server.send(200, "application/json", json);
}


void setup() {
  Serial.begin(115200);

  // Wi-Fi設定の最適化
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true); // 自動再接続を有効化
  WiFi.begin(ssid, password);

  Serial.print("WiFi接続中");
  configTime(9 * 3600, 0, "ntp.nict.jp", "ntp.jst.mfeed.ad.jp");  // 日本時間のNTP設定

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi接続成功");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.on("/set", handleSet); 
  server.on("/fan_toggle", handleFanToggle);
  server.on("/off", handleOff);
  server.on("/status", handleStatus);
  server.on("/fan_status", handleFanStatus);
  server.on("/lastlog", handleLastLog);
  server.on("/mode_toggle", handleModeToggle);
  server.on("/mode_status", handleModeStatus);
  server.begin();
  Serial.println("Webサーバ起動完了");
  

  ac.begin();  // IR送信用初期化
  irsend.begin();
}

void loop() {
  // Wi-Fi接続状態の監視と再接続
  if (WiFi.status() != WL_CONNECTED) {
    static unsigned long lastReconnectAttempt = 0;
    unsigned long now = millis();
    
    // 30秒おきに再接続を試みる
    if (now - lastReconnectAttempt > 30000) {
      Serial.println("WiFi disconnected. Reconnecting...");
      WiFi.disconnect();
      WiFi.begin(ssid, password);
      lastReconnectAttempt = now;
    }
  } else {
    server.handleClient();
  }
  
  // システムの安定性のために微小な待機を入れる
  delay(1);
}

