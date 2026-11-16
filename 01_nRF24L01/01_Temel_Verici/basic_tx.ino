#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

#define CE_PIN 7
#define CSN_PIN 8

RF24 radio(CE_PIN, CSN_PIN);
const byte address[6] = "00001";

struct VeriPaketi {
  byte cihazID;
  float deger;
  char mesaj[20];
};

VeriPaketi gonderilecekVeri;
float sayac = 0.0;

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
  Serial.println("Verici hazir...");
}

void loop() {
  gonderilecekVeri.cihazID = 1;
  gonderilecekVeri.deger = sayac;
  strcpy(gonderilecekVeri.mesaj, "Merhaba Dunya");

  bool iletildi = radio.write(&gonderilecekVeri, sizeof(VeriPaketi));

  if (iletildi) {
    Serial.print("Gonderildi -> Deger: ");
    Serial.println(sayac);
  } else {
    Serial.println("UYARI: Paket gonderilemedi!");
  }

  sayac += 1.0;
  delay(1000);
}
