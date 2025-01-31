#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
#include <Wire.h>
#include <SPI.h>
#include <LoRa.h>
#include <SD.h>

Adafruit_BMP280 bmp; // Objeto do BMP280

#define SEALEVELPRESSURE_HPA (1013.25) // Pressão ao nível do mar
#define CO2CILINDER_PIN 8
#define LINECUTTER1_PIN 9
#define LINECUTTER2_PIN 10
#define BMP280_ADRESS 0x76

// RFM95W
#define SS_PIN 5        // Pino Chip Select (NSS) do RFM95W
#define RST_PIN 14      // Pino de reset do RFM95W
#define DIO0_PIN 2      // Pino de interrupção do RFM95W (DIO0)

// NEO-M8N (SPI)
#define GPS_SS_PIN 4    // Chip Select do GPS
#define GPS_SCK_PIN 18  // Clock do GPS
#define GPS_MISO_PIN 19 // MISO do GPS
#define GPS_MOSI_PIN 23 // MOSI do GPS

// SD Card
#define SD_CS_PIN 15    // Pino Chip Select do módulo microSD

void setup() {
  Serial.begin(115200);

  // Inicializa pinos de controle
  pinMode(CO2CILINDER_PIN, OUTPUT);
  pinMode(LINECUTTER1_PIN, OUTPUT);
  pinMode(LINECUTTER2_PIN, OUTPUT);
  digitalWrite(CO2CILINDER_PIN, LOW);
  digitalWrite(LINECUTTER1_PIN, LOW);
  digitalWrite(LINECUTTER2_PIN, LOW);

  // Inicializa o sensor BMP280
  if (!bmp.begin(BMP280_ADRESS)) {
    Serial.println("Erro: Sensor BMP280 não encontrado!");
    while (1);
  }

  // Configurações do BMP280
  bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,
                  Adafruit_BMP280::SAMPLING_X2,    // Oversampling temperatura
                  Adafruit_BMP280::SAMPLING_X16,   // Oversampling pressão
                  Adafruit_BMP280::FILTER_X16,     // Filtro
                  Adafruit_BMP280::STANDBY_MS_500);// Tempo de espera entre medições

  // Inicializa o SPI para o GPS
  SPI.begin(GPS_SCK_PIN, GPS_MISO_PIN, GPS_MOSI_PIN, GPS_SS_PIN);
  pinMode(GPS_SS_PIN, OUTPUT);
  digitalWrite(GPS_SS_PIN, HIGH);

  // Inicializa o módulo LoRa (RFM95W)
  LoRa.setPins(SS_PIN, RST_PIN, DIO0_PIN);
  if (!LoRa.begin(868E6)) {
    Serial.println("Falha ao iniciar o LoRa!");
    while (1);
  }

  // Inicializa o cartão microSD
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("Erro ao inicializar o cartão SD!");
    while (1);
  }
}

void loop() {
  // Lê a pressão, temperatura e altitude
  float temperature = bmp.readTemperature();
  float pressure = bmp.readPressure() / 100.0F;
  float altitude = bmp.readAltitude(SEALEVELPRESSURE_HPA);

  bool co2Deployed = false;
  bool lineCutter1Deployed = false;
  bool lineCutter2Deployed = false;

  // Acionamento baseado na altitude
  if (altitude >= 3000) {
    Serial.println("A altitude de 3000 metros foi alcançada!");
    digitalWrite(CO2CILINDER_PIN, HIGH);
    co2Deployed = true;
    digitalWrite(LINECUTTER1_PIN, HIGH);
    lineCutter1Deployed = true;
  }

  if (altitude <= 200 && co2Deployed) {
    digitalWrite(LINECUTTER2_PIN, HIGH);
    lineCutter2Deployed = true;
  }

  // Lê dados do GPS via SPI
  digitalWrite(GPS_SS_PIN, LOW);
  String gpsData = "";
  while (SPI.transfer(0x00) != '\n') {
    gpsData += (char)SPI.transfer(0x00);
  }
  digitalWrite(GPS_SS_PIN, HIGH);

  // Processa os dados do GPS
  if (gpsData.length() > 0) {
    Serial.println("Dados do GPS recebidos: " + gpsData);
    // Criando mensagem LoRa
    String message = "GPS Data: " + gpsData + " Alt: " + String(altitude);
    // Enviar os dados via LoRa
    sendLoRaMessage(message);
    // Armazena os dados no microSD
    logDataToSD(temperature, pressure, altitude, gpsData);
  }

  Serial.println("----------------------");
  delay(2000);
}

// Função para enviar a mensagem via LoRa
void sendLoRaMessage(String message) {
  LoRa.beginPacket();
  LoRa.print(message);
  LoRa.endPacket();
  Serial.println("Mensagem enviada via LoRa: " + message);
}

// Função para armazenar os dados no microSD
void logDataToSD(float temperature, float pressure, float altitude, String gpsData) {
  File dataFile = SD.open("datalog.txt", FILE_WRITE);

  if (dataFile) {
    dataFile.print("Temperatura: ");
    dataFile.print(temperature);
    dataFile.print(" °C, Pressão: ");
    dataFile.print(pressure);
    dataFile.print(" hPa, Altitude: ");
    dataFile.print(altitude);
    dataFile.print(" m, GPS: ");
    dataFile.println(gpsData);
    dataFile.close();
    Serial.println("Dados armazenados no SD.");
  } else {
    Serial.println("Erro ao escrever no cartão SD!");
  }
}
 
