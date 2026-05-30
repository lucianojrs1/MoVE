// Necessário baixar
#include <ArduinoJson.h>        // ArduinJson by Benoit Blanchon
#include <ESP32-TWAI-CAN.hpp>   // ESP32-TWAI-CAN by sorek.uk
#include <WebSocketsClient.h>   // WbSockets by Markus Sattler
// Nativas
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <string.h>
#include <stdarg.h> // para logMessage
#include "../../config/constants.h"


// ------------------------------------------------------------------
// --- CONFIGURAÇÃO DE PINOS E VELOCIDADE ---
// ------------------------------------------------------------------
#define CAN_TX_PIN 5
#define CAN_RX_PIN 4
const TwaiSpeed CAN_SPEED = TWAI_SPEED_250KBPS;
// Flags
#define TESTMODE true
#define DEBUGMODE true

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
#define BUFFER_LENGTH 1000
// Configurações de rede
const char *ssid = "Salvacao_2_conto";
const char *password = "mimda2conto";
const char *serverAddress = "192.168.1.160";
const uint16_t serverPort = 3001;

WebSocketsClient webSocket;

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

void enviarFrameViaWebSocket(const CanMessage &frame) {
  if (!webSocket.isConnected()) return;

  StaticJsonDocument<256> doc;
  doc["type"] = "canFrame";
  doc["id"] = frame.id;
  doc["dlc"] = frame.length;
  doc["extended"] = frame.isExtended;

  JsonArray data = doc.createNestedArray("data");
  for (int i = 0; i < frame.length; i++) {
    data.add(frame.data[i]);
  }

  String jsonString;
  serializeJson(doc, jsonString);
  webSocket.sendTXT(jsonString);

  //logMessage("Frame CAN enviado via WebSocket");
  if (DEBUGMODE) {
    //logMessage("%s", jsonString.c_str());
  }
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
  static char lastMessage[MAX_LOG_MESSAGE_LEN] = {0}; // Armazena a última mensagem
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

void canSimTask(void *pvParameters) {
  const unsigned long SIM_INTERVAL_MS = 15; // ~66 frames/segundo
  while (true) {
    // Gera frame simulado
    CanMessage frame;
    frame.id = (random(1, 100) < 70)
        ? (random(0, 2) == 0 ? BASE_BATTERY_ID : BASE_CONTROLLER_ID)
        : random(0x000, 0x7FF + 1);
    frame.length = 8;
    frame.isExtended = false;
    for (int i = 0; i < 8; i++) {
      frame.data[i] = random(0, 256);
    }
    // Envia diretamente para a fila CAN
    if (xQueueSend(canFrameQueue, &frame, 10 / portTICK_PERIOD_MS) != pdTRUE) {
      logMessage("Fila CAN cheia (simulação)");
    }
    vTaskDelay(SIM_INTERVAL_MS / portTICK_PERIOD_MS);
  }
}

void debugTask(void *pvParameters) {
  const unsigned long DEBUG_INTERVAL_MS = 1000; // 1 segundos
  while (true) {
      // --- Status da Fila CAN ---
      if (canFrameQueue != NULL) {
        UBaseType_t queueItems = uxQueueMessagesWaiting(canFrameQueue);
        UBaseType_t queueSpaces = uxQueueSpacesAvailable(canFrameQueue);
        UBaseType_t queueLength = BUFFER_LENGTH;
        logMessage("--- Status da Fila CAN ---");
        logMessage("Itens na fila: %d", queueItems);
        logMessage("Espaços disponíveis: %d", queueSpaces);
        logMessage("Capacidade total: %d", queueLength);
        logMessage("Ocupação: %d%%", (queueItems * 100) / queueLength);
        logMessage("------------------------");
        if (queueItems == queueLength) {
          logMessage("ALERTA: Fila CAN está cheia!");
        } else if (queueItems > queueLength * 0.8) {
          logMessage("ALERTA: Fila CAN com alta ocupação (>80%)!");
        }
      }
    vTaskDelay(DEBUG_INTERVAL_MS / portTICK_PERIOD_MS);
  }
}

void canTask(void *pvParameters) {
  twai_message_t rxFrame;
  while (true) {
    bool frameLido = ESP32Can.readFrame(&rxFrame);
    if (frameLido) {
      CanMessage frameGenerico;
      frameGenerico.id = rxFrame.identifier;
      frameGenerico.length = rxFrame.data_length_code;
      frameGenerico.isExtended = (rxFrame.flags & TWAI_MSG_FLAG_EXTD) != 0;
      memcpy(frameGenerico.data, rxFrame.data, rxFrame.data_length_code);

      if (xQueueSend(canFrameQueue, &frameGenerico, 10 / portTICK_PERIOD_MS) != pdTRUE) {
        logMessage("Fila CAN cheia, descartando frame");
      }
    }
    vTaskDelay(15 / portTICK_PERIOD_MS);
  }
}

void webSocketTask(void *pvParameters) {
  while (true) {
    webSocket.loop();

    // Processa todos os frames na fila
    CanMessage frame;
    while (xQueueReceive(canFrameQueue, &frame, 0) == pdTRUE) {
      enviarFrameViaWebSocket(frame);
    }

    vTaskDelay(50 / portTICK_PERIOD_MS); 
  }
}

// ------------------------------------------------------------------
// --- SETUP E LOOP ---
// ------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);
  // Inicializa fila de logs
  logMessageQueue = xQueueCreate(20, MAX_LOG_MESSAGE_LEN);
  if (logMessageQueue == NULL) {
    Serial.println("ERRO: Falha ao criar fila de logs!");
    return;
  }

  dataMutex = xSemaphoreCreateMutex();
  if (dataMutex == NULL) {
    Serial.println("ERRO: Falha ao criar mutex!");
    return;
  }

  canFrameQueue = xQueueCreate(BUFFER_LENGTH, sizeof(CanMessage));
  if (canFrameQueue == NULL) {
    Serial.println("ERRO: Falha ao criar fila CAN!");
    return;
  }

  ESP32Can.setPins(CAN_TX_PIN, CAN_RX_PIN);
  if (ESP32Can.begin(CAN_SPEED)) {
    logMessage("Controlador CAN (TWAI) iniciado com sucesso!");
    logMessage("Monitorando em 250 kbps nos pinos TX:5 e RX:4...");
  } else {
    Serial.println("ERRO: Falha ao iniciar o controlador CAN! Verifique as conexões.");
    while (1) delay(100);
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
  
  // Cria tasks com base no modo
  if (TESTMODE) {
    logMessage("[INFO] Modo de simulação ativo");
    xTaskCreate(canSimTask, "CAN Sim Task", 4096, NULL, 2, NULL);
  } else {
    logMessage("[INFO] Modo CAN real ativo");
    xTaskCreate(canTask, "CAN Task", 4096, NULL, 2, NULL);
  }
  if( DEBUGMODE ){
    xTaskCreate(debugTask, "Debug Task", 2048, NULL, 0, NULL); 
  }
  xTaskCreate(webSocketTask, "WebSocket Task", 4096, NULL, 2, NULL);
  xTaskCreate(serialLoggerTask, "Serial Logger", 2048, NULL, 0, NULL);
  logMessage("------ Setup completo - Tasks rodando ------");
}

void loop() {
  vTaskDelay(1000 / portTICK_PERIOD_MS);
}