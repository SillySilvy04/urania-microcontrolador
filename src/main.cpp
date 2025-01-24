#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
#include <Wire.h>
#include <SPI.h>
#include <LoRa.h>

Adafruit_BMP280 bmp;//objeto do bmp280
#define SEALEVELPRESSURE_HPA (1013.25)//pressão em hPa
#define CO2CILINDER_PIN 8
#define LINECUTTER1_PIN 9
#define LINECUTTER2_PIN 10
#define BMP280_ADRESS 0x76

//RFM95W
#define SS_PIN 5        // Pino Chip Select (NSS) do RFM95W
#define RST_PIN 14      // Pino de reset do RFM95W
#define DIO0_PIN 2      // Pino de interrupção do RFM95W (DIO0)

//NEO-M8N (SPI)
#define GPS_SS_PIN 4    // Chip Select do GPS
#define GPS_SCK_PIN 18  // Clock do GPS
#define GPS_MISO_PIN 19 // MISO do GPS
#define GPS_MOSI_PIN 23 // MOSI do GPS

void setup() {
  //bmp280
  Serial.begin(115200);

  // Inicializa o pino CO2CILINDER_PIN como saída
  pinMode(CO2CILINDER_PIN, OUTPUT);
  pinMode(LINECUTTER1_PIN, OUTPUT);
  pinMode(LINECUTTER2_PIN, OUTPUT);
  digitalWrite(CO2CILINDER_PIN, LOW);  // Garante que o skib esteja desligado inicialmente
  digitalWrite(LINECUTTER1_PIN, LOW);  // Garante que o skib esteja desligado inicialmente
  digitalWrite(LINECUTTER2_PIN, LOW);  // Garante que o skib esteja desligado inicialmente

  // Inicia o sensor BMP280
  if (!bmp.begin(BMP280_ADRESS)) { // Endereço I²C padrão
    Serial.println("Erro: Sensor BMP280 não encontrado!");
    while (1);
  }

  // Configurações do BMP280
  bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,
                  Adafruit_BMP280::SAMPLING_X2,    // Temperatura oversampling
                  Adafruit_BMP280::SAMPLING_X16,   // Pressão oversampling
                  Adafruit_BMP280::FILTER_X16,     // Filtro
                  Adafruit_BMP280::STANDBY_MS_500);// Tempo de espera entre medições

  // Inicializa o SPI para o GPS
  SPI.begin(GPS_SCK_PIN, GPS_MISO_PIN, GPS_MOSI_PIN, GPS_SS_PIN);
  pinMode(GPS_SS_PIN, OUTPUT);
  digitalWrite(GPS_SS_PIN, HIGH);

  // RFM95W
  LoRa.setPins(SS_PIN, RST_PIN, DIO0_PIN);
  if (!LoRa.begin(868E6)) {  // Frequência de operação, pode ser 433E6, 868E6, ou 915E6 dependendo da região
    Serial.println("Falha ao iniciar o LoRa!");
    while (1);
  }
  Serial.println("LoRa inicializado com sucesso!");
}

void loop() {
  // Lê a pressão, temperatura e altitude
  float temperature = bmp.readTemperature(); // Temperatura em °C
  float pressure = bmp.readPressure() / 100.0F; // Pressão em hPa
  float altitude = bmp.readAltitude(SEALEVELPRESSURE_HPA); // Altitude em metros
  bool co2Deployed = false;
  bool lineCutter1Deployed = false;
  bool lineCutter2Deployed = false;

  // Verifica se a altitude alcançou ou ultrapassou 3000 metros
  if (altitude >= 3000) {
    Serial.println("A altitude de 3000 metros foi alcançada!");
    // Aciona o cilindro de co2 elétrico (pino HIGH)
    digitalWrite(CO2CILINDER_PIN, HIGH);
    co2Deployed = true;
    digitalWrite(LINECUTTER1_PIN, HIGH);
    lineCutter1Deployed = true;
  }

  if (altitude <= 2800 && co2Deployed){
    digitalWrite(LINECUTTER2_PIN, HIGH);
    lineCutter2Deployed = true;
  }

  // Lê dados do GPS via SPI
  digitalWrite(GPS_SS_PIN, LOW);
  String gpsData = "";
  while (SPI.transfer(0x00) != '\n') { // Leitura até o caractere de nova linha
    gpsData += (char)SPI.transfer(0x00);
  }
  digitalWrite(GPS_SS_PIN, HIGH);

  // Processa os dados do GPS
  if (gpsData.length() > 0) {
    Serial.println("Dados do GPS recebidos: " + gpsData);

    // Enviar os dados via LoRa a cada 2 segundos
    String message = "GPS Data: " + gpsData + " Alt: " + String(altitude);
    sendLoRaMessage(message);
  }

  Serial.println("----------------------");
  delay(2000); // Aguarda 2 segundos antes de medir novamente
}

// Função para enviar a mensagem via LoRa
void sendLoRaMessage(String message) {
  LoRa.beginPacket();
  LoRa.print(message);
  LoRa.endPacket();
  Serial.println("Mensagem enviada via LoRa: " + message);
}
