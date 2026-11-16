#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <Crypto.h>
#include <AES.h>

#define CE_PIN 7
#define CSN_PIN 8

RF24 radio(CE_PIN, CSN_PIN);
const byte address[6] = "00001";

// 256-bit (32 Bayt) AES Anahtarı (Alıcı ile BİREBİR AYNI olmalıdır)
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

float sayac = 0.0;

void aesECBSifrele(byte* veri) {
  aes256.setKey(aesKey, 32);
  aes256.encryptBlock(veri, veri);
  aes256.encryptBlock(veri + 16, veri + 16);
}

void setup() {
  Serial.begin(9600);

  if (!radio.begin()) {
    Serial.println("HATA: nRF24L01 baslatilamadi!");
    while (1);
  }

  radio.setChannel(22); // 2422 MHz
  radio.openWritingPipe(address);
  radio.setPALevel(RF24_PA_LOW);
  radio.stopListening();

  Serial.println("AES-256 Verici Hazir! Kriptolu gonderim basliyor...");
}

void loop() {
  byte paketTamponu[32] = {0};
  VeriPaketi* gonderilecek = (VeriPaketi*)paketTamponu;

  gonderilecek->cihazID = 1;
  gonderilecek->deger = sayac;
  strcpy(gonderilecek->mesaj, "Merhaba Dunya");

  aesECBSifrele(paketTamponu);

  bool iletildi = radio.write(paketTamponu, 32);

  if (iletildi) {
    Serial.print("Basariyla Gonderildi (AES-256) -> Deger: ");
    Serial.println(sayac);
  } else {
    Serial.println("Paket gonderilemedi! (Alici acik mi?)");
  }

  sayac += 1.0;
  delay(1000);
}
