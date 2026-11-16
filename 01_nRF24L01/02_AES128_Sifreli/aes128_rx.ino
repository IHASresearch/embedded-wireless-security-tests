#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <Crypto.h>
#include <AES.h>

#define CE_PIN 7
#define CSN_PIN 8

RF24 radio(CE_PIN, CSN_PIN);
const byte address[6] = "00001";

// 128-bit AES Anahtarı (Verici ile birebir aynı olmalı)
const byte aesKey[16] = {
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
  0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F
};

AES128 aes128;

struct __attribute__((packed)) VeriPaketi {
  int cihazID;        // 2 bayt
  float deger;        // 4 bayt
  char mesaj[26];     // 26 bayt -> Toplam: 32 bayt
};

// AES-128 ECB Şifre Çözme Fonksiyonu
void aesECBSifreCoz(byte* veri) {
  aes128.setKey(aesKey, 16);
  aes128.decryptBlock(veri, veri);           // İlk 16 bayt
  aes128.decryptBlock(veri + 16, veri + 16); // İkinci 16 bayt
}

void setup() {
  Serial.begin(9600);

  if (!radio.begin()) {
    Serial.println("HATA: nRF24L01 baslatilamadi!");
    while (1);
  }

  radio.setChannel(22);
  radio.openReadingPipe(0, address);
  radio.setPALevel(RF24_PA_LOW);
  radio.startListening();

  Serial.println("2422 MHz - AES-128 ECB Alici Dinlemede...");
}

void loop() {
  if (radio.available()) {
    byte sifreliTampon[32] = {0};
    byte cozulenTampon[32] = {0};

    // 1. Havadan gelen 32 baytlık şifreli veriyi oku
    radio.read(sifreliTampon, 32);

    // Şifreli ham veriyi korumak için çözme tamponuna kopyala
    memcpy(cozulenTampon, sifreliTampon, 32);

    // 2. Kopyalanan tampon üzerinde şifreyi çöz
    aesECBSifreCoz(cozulenTampon);

    // 3. Şifresi çözülmüş veriyi struct yapısına dönüştür
    VeriPaketi* cozulen = (VeriPaketi*)cozulenTampon;

    // --- SERİ MONİTÖR ÇIKTISI ---
    Serial.println("==========================================");

    // A) KRİPTOLU HAM VERİ (HEX Formatında)
    Serial.print("KRİPTOLU MESAJ (HEX) : ");
    for (int i = 0; i < 32; i++) {
      if (sifreliTampon[i] < 0x10) Serial.print("0");
      Serial.print(sifreliTampon[i], HEX);
      Serial.print(" ");
    }
    Serial.println();

    Serial.println("------------------------------------------");

    // B) KRİPTOSU ÇÖZÜLMÜŞ METİN VE VERİLER
    Serial.println("ÇÖZÜLEN VERİ PAKETİ:");
    Serial.print("Cihaz ID : ");
    Serial.println(cozulen->cihazID);

    Serial.print("Deger    : ");
    Serial.println(cozulen->deger);

    Serial.print("Mesaj    : ");
    Serial.println(cozulen->mesaj);

    Serial.println("==========================================");
    Serial.println();
  }
}
