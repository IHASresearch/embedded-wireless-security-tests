/*
 * ESP8266 Wi-Fi Paket Düşürme & Telemetri Testi - CLIENT (Partner Cihazı)
 *
 * Özellikler:
 * - Server'a (ESP8266_DROP_TEST) bağlanır.
 * - "Her pakette 1 paket düşür" (%50 Drop Simülasyonu).
 * - Sinyal gücünü (RSSI dBm) ve Server telemetrisini alır.
 * - Seri Monitöre anlık CSV formatında yazar.
 * - LittleFS içine (/log.csv) kaydeder.
 * - http://<Client_IP>/log.csv adresinden tek tıkla CSV indirilebilir.
 */

#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <LittleFS.h>
#include <ESP8266WebServer.h>

const char* AP_SSID = "ESP8266_DROP_TEST";
const char* AP_PASS = "12345678";

IPAddress serverIP(192, 168, 4, 1);
const unsigned int SERVER_PORT = 4210;

WiFiUDP Udp;
ESP8266WebServer webServer(80);

unsigned long packetSequence = 0;
unsigned long totalSent = 0;
unsigned long totalDropped = 0;

const unsigned long SEND_INTERVAL_MS = 500;
unsigned long lastSendTime = 0;

int serverChannel = 1;
float serverTxPower = 20.5;
unsigned long serverRecv = 0;
unsigned long serverDrop = 0;
float serverRate = 0.0;

void logToCSV(unsigned long timestamp, unsigned long seq, const char* status,
              int rssi, int channel, float txPower,
              unsigned long sRecv, unsigned long sDrop, float sRate) {
  char csvLine[160];
  snprintf(csvLine, sizeof(csvLine), "%lu,%lu,%s,%d,%d,%.1f,%lu,%lu,%.1f",
           timestamp, seq, status, rssi, channel, txPower, sRecv, sDrop, sRate);

  Serial.println(csvLine);

  File f = LittleFS.open("/log.csv", "a");
  if (f) {
    f.println(csvLine);
    f.close();
  }
}

void handleDownloadCSV() {
  if (LittleFS.exists("/log.csv")) {
    File f = LittleFS.open("/log.csv", "r");
    webServer.streamFile(f, "text/csv");
    f.close();
  } else {
    webServer.send(404, "text/plain", "Henuz log dosyasi olusmadi.");
  }
}

void handleClearCSV() {
  LittleFS.remove("/log.csv");
  File f = LittleFS.open("/log.csv", "w");
  if (f) {
    f.println("Timestamp_ms,Packet_Seq,Status,RSSI_dBm,Channel,Server_TxPower_dBm,Server_Recv,Server_Drop,Server_DropRate_Pct");
    f.close();
  }
  webServer.send(200, "text/html", "<h2>Loglar sifirlandi!</h2><a href='/'>Geri Don</a>");
}

void handleRoot() {
  String html = "<html><head><meta charset='utf-8'><title>ESP8266 Telemetri</title></head>";
  html += "<body style='font-family:sans-serif; text-align:center; padding-top:40px;'>";
  html += "<h2>ESP8266 Paket Drop & Telemetri Kaydedici</h2>";
  html += "<p>Toplam: " + String(packetSequence) + " | Gonderilen: " + String(totalSent) + " | Drop: " + String(totalDropped) + "</p>";
  html += "<p><a href='/log.csv' style='display:inline-block; padding:12px 24px; background:#28a745; color:#fff; text-decoration:none; border-radius:5px; font-weight:bold;'>CSV Dosyasini Indir (.csv)</a></p>";
  html += "<p><a href='/clear' onclick='return confirm(\"Sifirlansin mi?\")' style='color:#dc3545;'>Loglari Temizle</a></p>";
  html += "</body></html>";
  webServer.send(200, "text/html", html);
}

void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  Serial.println("\n==================================================");
  Serial.println("   ESP8266 CLIENT & CSV TELEMETRI LOGGER BAŞLATILDI");
  Serial.println("==================================================");

  if (LittleFS.begin()) {
    Serial.println("[✓] LittleFS dosya sistemi hazir.");
    if (!LittleFS.exists("/log.csv")) {
      File f = LittleFS.open("/log.csv", "w");
      if (f) {
        f.println("Timestamp_ms,Packet_Seq,Status,RSSI_dBm,Channel,Server_TxPower_dBm,Server_Recv,Server_Drop,Server_DropRate_Pct");
        f.close();
      }
    }
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(AP_SSID, AP_PASS);

  Serial.print("Server AP'sine baglaniliyor ('");
  Serial.print(AP_SSID);
  Serial.print("')");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\n[✓] Baglanti kuruldu!");
  Serial.print("    Client IP : "); Serial.println(WiFi.localIP());
  Serial.printf("    Web Paneli : http://%s/\n", WiFi.localIP().toString().c_str());
  Serial.printf("    CSV Linki  : http://%s/log.csv\n", WiFi.localIP().toString().c_str());

  webServer.on("/", handleRoot);
  webServer.on("/log.csv", handleDownloadCSV);
  webServer.on("/clear", handleClearCSV);
  webServer.begin();

  Udp.begin(4211);

  Serial.println("\n--- [CSV LOG FORMATI BAŞLANGICI] ---");
  Serial.println("Timestamp_ms,Packet_Seq,Status,RSSI_dBm,Channel,Server_TxPower_dBm,Server_Recv,Server_Drop,Server_DropRate_Pct");
}

void loop() {
  webServer.handleClient();
  unsigned long currentMillis = millis();

  if (currentMillis - lastSendTime >= SEND_INTERVAL_MS) {
    lastSendTime = currentMillis;
    packetSequence++;

    int currentRSSI = WiFi.RSSI();
    bool shouldDrop = (packetSequence % 2 == 0);

    if (shouldDrop) {
      totalDropped++;
      logToCSV(currentMillis, packetSequence, "DROP", currentRSSI,
               serverChannel, serverTxPower, serverRecv, serverDrop, serverRate);

      digitalWrite(LED_BUILTIN, LOW);
      delay(20);
      digitalWrite(LED_BUILTIN, HIGH);
    } else {
      char packetData[64];
      snprintf(packetData, sizeof(packetData), "SEQ:%lu|TIME:%lu|DATA:PING",
               packetSequence, currentMillis);

      Udp.beginPacket(serverIP, SERVER_PORT);
      Udp.write((const uint8_t*)packetData, strlen(packetData));
      Udp.endPacket();

      totalSent++;
      logToCSV(currentMillis, packetSequence, "SENT", currentRSSI,
               serverChannel, serverTxPower, serverRecv, serverDrop, serverRate);

      digitalWrite(LED_BUILTIN, LOW);
      delay(40);
      digitalWrite(LED_BUILTIN, HIGH);
    }
  }

  int packetSize = Udp.parsePacket();
  if (packetSize) {
    char replyBuffer[128];
    int len = Udp.read(replyBuffer, sizeof(replyBuffer) - 1);
    if (len > 0) {
      replyBuffer[len] = '\0';
      String reply = String(replyBuffer);

      if (reply.startsWith("TELEMETRY|")) {
        int chIdx = reply.indexOf("CH:");
        if (chIdx != -1) serverChannel = reply.substring(chIdx + 3).toInt();

        int pwrIdx = reply.indexOf("TXPWR:");
        if (pwrIdx != -1) serverTxPower = reply.substring(pwrIdx + 6).toFloat();

        int recvIdx = reply.indexOf("RECV:");
        if (recvIdx != -1) serverRecv = reply.substring(recvIdx + 5).toInt();

        int dropIdx = reply.indexOf("DROP:");
        if (dropIdx != -1) serverDrop = reply.substring(dropIdx + 5).toInt();

        int rateIdx = reply.indexOf("RATE:");
        if (rateIdx != -1) serverRate = reply.substring(rateIdx + 5).toFloat();
      }
    }
  }

  yield();
}
