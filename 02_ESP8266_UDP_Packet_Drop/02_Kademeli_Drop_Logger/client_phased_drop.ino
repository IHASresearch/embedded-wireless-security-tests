/*
 * ESP8266 Wi-Fi Kademeli Paket Düşürme Testi - CLIENT (Partner Cihazı)
 *
 * Test Akışı:
 * - 1. Aşama: Her 2 pakette 1 drop (%50 Kayıp)  -> 20 paket
 * - 2. Aşama: Her 4 pakette 1 drop (%25 Kayıp)  -> 20 paket
 * - 3. Aşama: Her 5 pakette 1 drop (%20 Kayıp)  -> 20 paket
 * - 4. Aşama: Her 10 pakette 1 drop (%10 Kayıp) -> 20 paket
 * (Tamamlandığında döngüsel olarak başa döner).
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

const int DROP_INTERVALS[] = {2, 4, 5, 10};
const char* PHASE_NAMES[] = {"2de1 (%50)", "4te1 (%25)", "5te1 (%20)", "10da1 (%10)"};
const int TOTAL_PHASES = 4;
const int PACKETS_PER_PHASE = 20;

int currentPhaseIdx = 0;
int phasePacketCounter = 0;

int serverChannel = 1;
float serverTxPower = 20.5;
unsigned long serverRecv = 0;
unsigned long serverDrop = 0;
float serverRate = 0.0;

void logToCSV(unsigned long timestamp, unsigned long seq, const char* phase, int dropInterval,
              const char* status, int rssi, int channel, float txPower,
              unsigned long sRecv, unsigned long sDrop, float sRate) {
  char csvLine[180];
  snprintf(csvLine, sizeof(csvLine), "%lu,%lu,%s,%d,%s,%d,%d,%.1f,%lu,%lu,%.1f",
           timestamp, seq, phase, dropInterval, status, rssi, channel, txPower, sRecv, sDrop, sRate);

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
    f.println("Timestamp_ms,Packet_Seq,Phase,Drop_Interval,Status,RSSI_dBm,Channel,Server_TxPower_dBm,Server_Recv,Server_Drop,Server_DropRate_Pct");
    f.close();
  }
  webServer.send(200, "text/html", "<h2>Loglar sifirlandi!</h2><a href='/'>Geri Don</a>");
}

void handleRoot() {
  String html = "<html><head><meta charset='utf-8'><title>Client Kademeli Drop Paneli</title></head>";
  html += "<body style='font-family:sans-serif; text-align:center; padding-top:40px;'>";
  html += "<h2>ESP8266 Kademeli Drop Testi</h2>";
  html += "<p><b>Aktif Asama:</b> " + String(PHASE_NAMES[currentPhaseIdx]) + "</p>";
  html += "<p>Toplam Uretilen: " + String(packetSequence) + " | Gonderilen: " + String(totalSent) + " | Drop: " + String(totalDropped) + "</p>";
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
  Serial.println("   ESP8266 CLIENT (KADEMELİ DROP TESTİ)");
  Serial.println("==================================================");

  if (LittleFS.begin()) {
    Serial.println("[✓] LittleFS dosya sistemi hazir.");
    if (!LittleFS.exists("/log.csv")) {
      File f = LittleFS.open("/log.csv", "w");
      if (f) {
        f.println("Timestamp_ms,Packet_Seq,Phase,Drop_Interval,Status,RSSI_dBm,Channel,Server_TxPower_dBm,Server_Recv,Server_Drop,Server_DropRate_Pct");
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
  Serial.println("Timestamp_ms,Packet_Seq,Phase,Drop_Interval,Status,RSSI_dBm,Channel,Server_TxPower_dBm,Server_Recv,Server_Drop,Server_DropRate_Pct");
}

void loop() {
  webServer.handleClient();
  unsigned long currentMillis = millis();

  if (currentMillis - lastSendTime >= SEND_INTERVAL_MS) {
    lastSendTime = currentMillis;
    packetSequence++;
    phasePacketCounter++;

    int currentInterval = DROP_INTERVALS[currentPhaseIdx];
    const char* currentPhaseName = PHASE_NAMES[currentPhaseIdx];
    int currentRSSI = WiFi.RSSI();

    bool shouldDrop = (phasePacketCounter % currentInterval == 0);

    if (shouldDrop) {
      totalDropped++;
      logToCSV(currentMillis, packetSequence, currentPhaseName, currentInterval,
               "DROP", currentRSSI, serverChannel, serverTxPower, serverRecv, serverDrop, serverRate);

      digitalWrite(LED_BUILTIN, LOW);
      delay(20);
      digitalWrite(LED_BUILTIN, HIGH);
    } else {
      char packetData[100];
      snprintf(packetData, sizeof(packetData), "SEQ:%lu|PHASE:%s|DATA:PING",
               packetSequence, currentPhaseName);

      Udp.beginPacket(serverIP, SERVER_PORT);
      Udp.write((const uint8_t*)packetData, strlen(packetData));
      Udp.endPacket();

      totalSent++;
      logToCSV(currentMillis, packetSequence, currentPhaseName, currentInterval,
               "SENT", currentRSSI, serverChannel, serverTxPower, serverRecv, serverDrop, serverRate);

      digitalWrite(LED_BUILTIN, LOW);
      delay(40);
      digitalWrite(LED_BUILTIN, HIGH);
    }

    if (phasePacketCounter >= PACKETS_PER_PHASE) {
      phasePacketCounter = 0;
      currentPhaseIdx = (currentPhaseIdx + 1) % TOTAL_PHASES;
      Serial.printf("\n>>> [AŞAMA DEĞİŞTİ] Yeni Kural: %s <<<\n\n", PHASE_NAMES[currentPhaseIdx]);
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
