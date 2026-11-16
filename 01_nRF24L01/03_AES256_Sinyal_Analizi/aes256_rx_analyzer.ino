#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <Crypto.h>
#include <AES.h>

#define CE_PIN 7
#define CSN_PIN 8

RF24 radio(CE_PIN, CSN_PIN);
const byte address[6] = "00001";

const byte aesKey[32] = {
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
  0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
  0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
  0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F
};

AES256 aes256;

struct __attribute__((packed)) VeriPaketi {
  int cihazID;        // 2 bayt
  float deger;        // 4 bayt
  char mesaj[26];     // 26 bayt -> Toplam: 32 bayt
};

void aesECBSifreCoz(byte* veri) {
  aes256.setKey(aesKey, 32);
  aes256.decryptBlock(veri, veri);
  aes256.decryptBlock(veri + 16, veri + 16);
}

void setup() {
  Serial.begin(9600);

  if (!radio.begin()) {
    Serial.println("HATA: nRF24L01 baslatilamadi!");
    while (1);
  }

  radio.setChannel(22); // 2422 MHz
  radio.openReadingPipe(0, address);
  radio.setPALevel(RF24_PA_LOW);
  radio.startListening();

  Serial.println("==============================================");
  Serial.println("2422 MHz - AES-256 ECB Alici & RF Analizör Hazir");
  Serial.print("Aktif Frekans   : ");
  Serial.print(2400 + radio.getChannel());
  Serial.println(" MHz");
  Serial.print("Veri Hizi (Speed): ");
  Serial.print(radio.getDataRate() == RF24_1MBPS ? "1 Mbps" :
               (radio.getDataRate() == RF24_2MBPS ? "2 Mbps" : "250 kbps"));
  Serial.println();
  Serial.println("==============================================\n");
}

void loop() {
  if (radio.available()) {
    byte sifreliTampon[32] = {0};
    byte cozulenTampon[32] = {0};

    bool sinyalYuksek = radio.testRPD();

    radio.read(sifreliTampon, 32);
    memcpy(cozulenTampon, sifreliTampon, 32);

    aesECBSifreCoz(cozulenTampon);
    VeriPaketi* cozulen = (VeriPaketi*)cozulenTampon;

    Serial.println("==========================================");
    Serial.println("[ RF KANAL VE SİNYAL ANALİZİ ]");
    Serial.print("Kanal / Frekans  : Kanal ");
    Serial.print(radio.getChannel());
    Serial.print(" (");
    Serial.print(2400 + radio.getChannel());
    Serial.println(" MHz)");

    Serial.print("Sinyal Gücü Eşiği: ");
    if (sinyalYuksek) {
      Serial.println("GÜÇLÜ (>-64 dBm)");
    } else {
      Serial.println("ZAYIF (<-64 dBm)");
    }

    Serial.println("------------------------------------------");
    Serial.print("KRİPTOLU MESAJ (HEX) : ");
    for (int i = 0; i < 32; i++) {
      if (sifreliTampon[i] < 0x10) Serial.print("0");
      Serial.print(sifreliTampon[i], HEX);
      Serial.print(" ");
    }
    Serial.println();

    Serial.println("------------------------------------------");
    Serial.println("ÇÖZÜLEN VERİ PAKETİ (AES-256):");
    Serial.print("Cihaz ID : ");
    Serial.println(cozulen->cihazID);

    Serial.print("Deger    : ");
    Serial.println(cozulen->deger);

    Serial.print("Mesaj    : ");
    Serial.println(cozulen->mesaj);
    Serial.println("==========================================\n");
  }
}
