#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <Crypto.h>
#include <AES.h>

#define CE_PIN 7
#define CSN_PIN 8
#define KANAL_ATLAMA_SURESI 1000 // 1 Saniye

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

// Paket Yapısı (Toplam: 2 + 4 + 26 = 32 Bayt)
struct __attribute__((packed)) VeriPaketi {
  int cihazID;        // 2 bayt
  float deger;        // 4 bayt
  char mesaj[26];     // 26 bayt
};

// 15 Kanallı FHSS Frekans Haritası (Alıcı ile BİREBİR AYNI olmalıdır)
const byte fhssKanallari[15] = {5, 12, 22, 35, 48, 55, 62, 70, 78, 85, 92, 100, 108, 115, 120};
byte kanalIndeks = 0;

unsigned long sonKanalDegisimZamani = 0;
unsigned long sonGonderimZamani = 0;
float sayac = 0.0;

// AES-256 ECB Şifreleme Fonksiyonu
void aesECBSifrele(byte* veri) {
  aes256.setKey(aesKey, 32);
  aes256.encryptBlock(veri, veri);           // İlk 16 bayt
  aes256.encryptBlock(veri + 16, veri + 16); // İkinci 16 bayt
}

void setup() {
  Serial.begin(9600);

  if (!radio.begin()) {
    Serial.println("HATA: nRF24L01 baslatilamadi!");
    while (1);
  }

  radio.setDataRate(RF24_1MBPS);
  radio.openWritingPipe(address);
  radio.setPALevel(RF24_PA_LOW);
  radio.stopListening();

  // Başlangıç kanalını ayarla
  radio.setChannel(fhssKanallari[kanalIndeks]);

  Serial.println("==============================================");
  Serial.println("FHSS + AES-256 Verici Hazir!");
  Serial.print("Baslangic Frekansi: ");
  Serial.print(2400 + fhssKanallari[kanalIndeks]);
  Serial.println(" MHz");
  Serial.println("==============================================\n");

  sonKanalDegisimZamani = millis();
  sonGonderimZamani = millis();
}

void loop() {
  unsigned long simdikiZaman = millis();

  // 1. 1 Saniyede Bir Frekans Atla (FHSS)
  if (simdikiZaman - sonKanalDegisimZamani >= KANAL_ATLAMA_SURESI) {
    sonKanalDegisimZamani = simdikiZaman;
    kanalIndeks = (kanalIndeks + 1) % 15;

    radio.setChannel(fhssKanallari[kanalIndeks]);

    Serial.print("\n>>> VERICI FREKANS ATLADI: Yeni Frekans -> ");
    Serial.print(2400 + fhssKanallari[kanalIndeks]);
    Serial.print(" MHz (Kanal ");
    Serial.print(fhssKanallari[kanalIndeks]);
    Serial.println(") <<<\n");
  }

  // 2. Her 800 ms'de bir paket gönder
  if (simdikiZaman - sonGonderimZamani >= 800) {
    sonGonderimZamani = simdikiZaman;

    byte paketTamponu[32] = {0};
    VeriPaketi* gonderilecek = (VeriPaketi*)paketTamponu;

    gonderilecek->cihazID = 1;
    gonderilecek->deger = sayac;
    strcpy(gonderilecek->mesaj, "FHSS Dunya");

    // Şifrele ve gönder
    aesECBSifrele(paketTamponu);
    bool iletildi = radio.write(paketTamponu, 32);

    if (iletildi) {
      Serial.print("Gonderildi [");
      Serial.print(2400 + fhssKanallari[kanalIndeks]);
      Serial.print(" MHz] -> Deger: ");
      Serial.println(sayac);
    } else {
      Serial.print("Gonderilemedi [");
      Serial.print(2400 + fhssKanallari[kanalIndeks]);
      Serial.println(" MHz]");
    }

    sayac += 1.0;
  }
}
