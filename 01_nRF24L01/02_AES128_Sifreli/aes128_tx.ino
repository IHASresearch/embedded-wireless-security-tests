#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <Crypto.h>
#include <AES.h>

#define CE_PIN 7
#define CSN_PIN 8

RF24 radio(CE_PIN, CSN_PIN);
const byte address[6] = "00001";

// 128-bit AES Anahtarı (Alıcı ile BİREBİR AYNI)
const byte aesKey[16] = {
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
  0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F
};

AESTiny128 aes128;

struct __attribute__((packed)) VeriPaketi {
  int cihazID;        // 2 bayt
  float deger;        // 4 bayt
  char mesaj[26];     // 26 bayt -> Toplam: 32 bayt
};

float sayac = 0.0;

// Şifreleme işlemini doğrudan bayt blokları üzerinde yapar
void aesECBSifrele(byte* veri) {
  aes128.setKey(aesKey, 16);
  aes128.encryptBlock(veri, veri);           // İlk 16 bayt
  aes128.encryptBlock(veri + 16, veri + 16); // Sonraki 16 bayt
}

void setup() {
  Serial.begin(9600);

  if (!radio.begin()) {
    Serial.println("HATA: nRF24L01 baslatilamadi!");
    while (1);
  }

  radio.setChannel(22);
  radio.openWritingPipe(address);
  radio.setPALevel(RF24_PA_LOW);
  radio.stopListening();

  Serial.println("Verici Hazir. Kriptolu gonderim basliyor...");
}

void loop() {
  byte paketTamponu[32] = {0};
  VeriPaketi* gonderilecek = (VeriPaketi*)paketTamponu;

  // 1. Verileri tampon üzerine doldur
  gonderilecek->cihazID = 1;
  gonderilecek->deger = sayac;
  strcpy(gonderilecek->mesaj, "Merhaba Dunya");

  // 2. Tamponu yerinde şifrele
  aesECBSifrele(paketTamponu);

  // 3. 32 baytlık veriyi gönder
  bool iletildi = radio.write(paketTamponu, 32);

  if (iletildi) {
    Serial.print("Basariyla Gonderildi -> Deger: ");
    Serial.println(sayac);
  } else {
    Serial.println("Paket gonderilemedi!");
  }

  sayac += 1.0;
  delay(1000);
}
