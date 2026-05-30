// Necessário baixar
#include <ArduinoJson.h>      // ArduinJson by Benoit Blanchon
#include <ESP32-TWAI-CAN.hpp> // ESP32-TWAI-CAN by sorek.uk
#include <WebSocketsClient.h> // WbSockets by Markus Sattler
// Nativas
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <stdarg.h> // para logMessage
#include <string.h>

#include <MPU6050_light.h>
#include <Wire.h>
#include "../../config/constants.h"

// ------------------------------------------------------------------
// --- CONFIGURAÇÃO DE PINOS E VELOCIDADE ---
// ------------------------------------------------------------------
#define CAN_TX_PIN 5
#define CAN_RX_PIN 4
const TwaiSpeed CAN_SPEED = TWAI_SPEED_250KBPS;
// Flags
#define TESTMODE false
#define DEBUGMODE false

// Estrutura para armazenar frames CAN genéricos
struct CanMessage {
  uint32_t id;
  uint8_t data[8];
  uint8_t length;
  bool isExtended;
};
#define WEBSOCKET_RECONNECT_INTERVAL 1000 // 1 segundos
unsigned long lastReconnectAttempt = 0;
// Fila de logs
#define MAX_LOG_MESSAGE_LEN 128
QueueHandle_t logMessageQueue;
// Mutex e fila CAN
SemaphoreHandle_t dataMutex;
QueueHandle_t canFrameQueue;

// Variáveis para armazenar os dados decodificados
struct BatteryData {
  int current = 0;
  int voltage = 0;
  int soc = 0;
  int soh = 0;
  int temperature = 0;
  bool valid = false;
} battery;

struct MotorControllerData {
  int motorSpeedRpm = 0;
  float motorTorque = 0.0;
  int motorTemperature = 0;
  int controllerTemperature = 0;
  bool valid = false;
} motorController;
// Variáveis para armazenar os dados anteriores
BatteryData batteryPrev = {};
MotorControllerData motorControllerPrev = {};

#define BUFFER_LENGTH 1000
// Configurações de rede
const char *ssid = "Salvacao_2_conto";
const char *password = "mimda2conto";
// http://voltzlab.ddns.net:3001/
const char *serverAddress = "192.168.1.160";
const uint16_t serverPort = 3000;

WebSocketsClient webSocket;
MPU6050 mpu(Wire);
unsigned long lastSend = 0;
bool mpuReady = false;

// ------------------------------------------------------------------
// --- FUNÇÃO DE LOG THREAD-SAFE ---
// ------------------------------------------------------------------
void logMessage(const char *fmt, ...) {
  char buffer[MAX_LOG_MESSAGE_LEN];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buffer, sizeof(buffer), fmt, args);
  va_end(args);

  // Envia para a fila de logs (não bloqueante)
  xQueueSendToBack(logMessageQueue, buffer, 0);
}
// ------------------------------------------------------------------
// --- FUNÇÕES AUXILIARES ---
// ------------------------------------------------------------------

void enviarFrameViaWebSocket() {
  if (!webSocket.isConnected())
    return;

  StaticJsonDocument<256> doc;
  doc["roll"] = String(mpu.getAngleX(), 2);
  doc["pitch"] = String(mpu.getAngleY(), 2);
  doc["yaw"] = String(mpu.getAngleZ(), 2);
  doc["accX"] = String(mpu.getAccX(), 2);
  doc["accY"] = String(mpu.getAccY(), 2);
  doc["accZ"] = String(mpu.getAccZ(), 2);
  doc["gyroX"] = String(mpu.getGyroX(), 2);
  doc["gyroY"] = String(mpu.getGyroY(), 2);
  doc["gyroZ"] = String(mpu.getGyroZ(), 2);
  doc["temp"] = String(mpu.getTemp(), 2);

  JsonArray data = doc.createNestedArray("data");

  String jsonString;
  serializeJson(doc, jsonString);
  webSocket.sendTXT(jsonString);
}

void webSocketEvent(WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
  case WStype_DISCONNECTED:
    logMessage("[WSc] Disconnected!");
    break;
  case WStype_CONNECTED:
    logMessage("[WSc] Connected to url: %s", (char *)payload);
    webSocket.sendTXT("ESP32 Conectado ao WebSocket!");
    break;
  case WStype_ERROR:
    logMessage("[WSc] Error: %s", (char *)payload);
    break;
  case WStype_TEXT:
    // logMessage("Received: %.*s", length, payload); // opcional
    break;
  }
}
// ------------------------------------------------------------------
// --- TAREFAS ---
// ------------------------------------------------------------------
void serialLoggerTask(void *pvParameters) {
  char buffer[MAX_LOG_MESSAGE_LEN];
  static char lastMessage[MAX_LOG_MESSAGE_LEN] = {
      0}; // Armazena a última mensagem
  while (true) {
    if (xQueueReceive(logMessageQueue, buffer, portMAX_DELAY) == pdTRUE) {
      // Compara com a última mensagem
      if (strcmp(buffer, lastMessage) == 0) {
        // Senão: silencia a repetição
      } else {
        // Mensagem nova: imprime e atualiza
        Serial.println(buffer);
        strcpy(lastMessage, buffer);
      }
    }
    vTaskDelay(1 / portTICK_PERIOD_MS);
  }
}

void webSocketTask(void *pvParameters) {
  while (true) {
    webSocket.loop();
    mpu.update();
    enviarFrameViaWebSocket();
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}

// ------------------------------------------------------------------
// --- SETUP E LOOP ---
// ------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  while (!Serial)
    delay(10);

  Wire.begin(5, 18); // SDA, SCL

  // Inicializa MPU-6050
  if (mpu.begin() != 0) {
    Serial.println("Erro no MPU-6050!");
    while (1)
      delay(100);
  }
  Serial.println("Calibrando (10s, mantenha parado)...");
  mpu.calcOffsets(true, true);
  mpuReady = true;
  Serial.println("✅ MPU-6050 pronto.");

  // Inicializa fila de logs
  logMessageQueue = xQueueCreate(20, MAX_LOG_MESSAGE_LEN);
  if (logMessageQueue == NULL) {
    Serial.println("ERRO: Falha ao criar fila de logs!");
    return;
  }

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  logMessage("WiFi connected!");
  logMessage("IP: %s", WiFi.localIP().toString().c_str());

  // Configuração WebSocket
  webSocket.begin(serverAddress, serverPort, "/");
  webSocket.onEvent(webSocketEvent);

  xTaskCreate(webSocketTask, "WebSocket Task", 4096, NULL, 2, NULL);
  xTaskCreate(serialLoggerTask, "Serial Logger", 2048, NULL, 0, NULL);
  logMessage("------ Setup completo - Tasks rodando ------");
}

void loop() { vTaskDelay(1000 / portTICK_PERIOD_MS); }
