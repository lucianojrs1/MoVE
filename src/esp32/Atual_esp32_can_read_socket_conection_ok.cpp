// Necessário baixar
#include <ArduinoJson.h>
#include <ESP32-TWAI-CAN.hpp>
#include <WebSocketsClient.h>
// Nativas
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <string.h>
// Flag para ativar modo de simulação de rede CAN
#define TESTMODE true
// Flag para ativar modo de debug da rede CAN
#define DEBUGMODE false
// ------------------------------------------------------------------
// --- CONFIGURAÇÃO DE PINOS E VELOCIDADE ---
// ------------------------------------------------------------------
#define CAN_TX_PIN 5
#define CAN_RX_PIN 4
const TwaiSpeed CAN_SPEED = TWAI_SPEED_250KBPS;

// IDs base (apenas para simulação, não decodificação)
#include "../../config/constants.h"


// Estrutura para armazenar frames CAN genéricos
struct CanMessage {
  uint32_t id;
  uint8_t data[8];
  uint8_t length;
  bool isExtended;
};

// Mutex e fila
SemaphoreHandle_t dataMutex;
QueueHandle_t canFrameQueue;

// Configurações de rede
const char *ssid = "Salvacao_2_conto";
const char *password = "mimda2conto";
const char *serverAddress = "192.168.1.160";
const uint16_t serverPort = 3001;

WebSocketsClient webSocket;

// ------------------------------------------------------------------
// --- FUNÇÕES AUXILIARES ---
// ------------------------------------------------------------------
void gerarFrameCanSimulado(twai_message_t &frame) {
  uint32_t randomId = random(0x000, 0x7FF + 1);
  if (random(0, 100) < 70) {
    frame.identifier = (random(0, 2) == 0) ? BASE_BATTERY_ID : BASE_CONTROLLER_ID;
  } else {
    frame.identifier = randomId;
  }
  frame.flags = 0;
  frame.data_length_code = 8;
  for (int i = 0; i < frame.data_length_code; i++) {
    frame.data[i] = random(0, 256);
  }
}

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

  Serial.println("Frame CAN enviado via WebSocket");
  if (DEBUGMODE) {
    Serial.println(jsonString);
  }
}

void webSocketEvent(WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.println("[WSc] Disconnected!");
      break;
    case WStype_CONNECTED:
      Serial.print("[WSc] Connected to url: ");
      Serial.println((char *)payload);
      webSocket.sendTXT("ESP32 conectado ao WebSocket!");
      break;
    case WStype_TEXT:
      // Tratar mensagens recebidas do servidor se necessário
      break;
    case WStype_ERROR:
      Serial.printf("[WSc] Error: %s\n", (char *)payload);
      break;
  }
}

// ------------------------------------------------------------------
// --- TAREFAS ---
// ------------------------------------------------------------------
void canTask(void *pvParameters) {
  twai_message_t rxFrame;
  unsigned long lastStatusCheck = 0;
  const unsigned long STATUS_CHECK_INTERVAL = 5000;  // 5 segundos

  while (true) {
    bool frameLido = ESP32Can.readFrame(&rxFrame);

    if (!frameLido && TESTMODE) {
      gerarFrameCanSimulado(rxFrame);
      frameLido = true;
      Serial.println("[Simulacao] Gerando frame CAN simulado.");
    }

    if (frameLido) {
      // Armazena frame genérico na fila
      CanMessage frameGenerico;
      frameGenerico.id = rxFrame.identifier;
      frameGenerico.length = rxFrame.data_length_code;
      frameGenerico.isExtended = (rxFrame.flags & TWAI_MSG_FLAG_EXTD) != 0;
      memcpy(frameGenerico.data, rxFrame.data, rxFrame.data_length_code);

      // Envia para a fila com timeout
      if (xQueueSend(canFrameQueue, &frameGenerico, 10 / portTICK_PERIOD_MS) != pdTRUE) {
        Serial.println("Fila CAN cheia, descartando frame");
      }
    }

    // ----DEBUGMODE ---- Verifica status da rede CAN e Fila periodicamente
    if (millis() - lastStatusCheck >= STATUS_CHECK_INTERVAL && DEBUGMODE) {
      twai_status_info_t status;
      if (twai_get_status_info(&status) == ESP_OK) {
        Serial.println("--- Status da Rede CAN ---");
        switch (status.state) {
          case TWAI_STATE_STOPPED: Serial.println("PARADA"); break;
          case TWAI_STATE_RUNNING: Serial.println("RODANDO"); break;
          case TWAI_STATE_BUS_OFF: Serial.println("BUS OFF"); break;
          case TWAI_STATE_RECOVERING: Serial.println("RECUPERANDO"); break;
        }

        Serial.printf("Erros TX: %d\n", status.tx_error_counter);
        Serial.printf("Erros RX: %d\n", status.rx_error_counter);
        Serial.printf("Frames TX: %d\n", status.tx_failed_count);
        Serial.printf("Frames RX: %d\n", status.rx_missed_count);
        Serial.printf("Frames RX FIFO cheio: %d\n", status.rx_overrun_count);
        Serial.println("------------------------");

        // Verifica se a rede está ativa
        if (status.state != TWAI_STATE_RUNNING) {
          Serial.println("ALERTA: Rede CAN não está em estado de operação normal!");
        }

        UBaseType_t queueItems = uxQueueMessagesWaiting(canFrameQueue);
        UBaseType_t queueSpaces = uxQueueSpacesAvailable(canFrameQueue);
        UBaseType_t queueLength = (UBaseType_t)50;  // Tamanho total da fila

        Serial.printf("--- Status da Fila CAN ---\n");
        Serial.printf("Itens na fila: %d\n", queueItems);
        Serial.printf("Espaços disponíveis: %d\n", queueSpaces);
        Serial.printf("Capacidade total: %d\n", queueLength);
        Serial.printf("Ocupação: %d%%\n", (queueItems * 100) / queueLength);
        Serial.println("------------------------");

        if (queueItems == queueLength) {
          Serial.println("ALERTA: Fila CAN está cheia!");
        } else if (queueItems > queueLength * 0.8) {
          Serial.println("ALERTA: Fila CAN com alta ocupação (>80%)!");
        }
      } else {
        Serial.println("Falha ao obter status da rede CAN");
      }
      lastStatusCheck = millis();
    }
    vTaskDelay(1 / portTICK_PERIOD_MS);
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

    vTaskDelay(10 / portTICK_PERIOD_MS);  // 10ms delay
  }
}

// ------------------------------------------------------------------
// --- SETUP E LOOP ---
// ------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);
  // Inicializa mutex
  dataMutex = xSemaphoreCreateMutex();
  if (dataMutex == NULL) {
    Serial.println("ERRO: Falha ao criar mutex!");
    return;
  }

  // Inicializa fila
  canFrameQueue = xQueueCreate(50, sizeof(CanMessage));
  if (canFrameQueue == NULL) {
    Serial.println("ERRO: Falha ao criar fila CAN!");
    return;
  }

  // Configuração CAN
  ESP32Can.setPins(CAN_TX_PIN, CAN_RX_PIN);
  if (ESP32Can.begin(CAN_SPEED)) {
    Serial.println("Controlador CAN (TWAI) iniciado com sucesso!");
    Serial.println("Monitorando em 250 kbps nos pinos TX:5 e RX:4...");
  } else {
    Serial.println("ERRO: Falha ao iniciar o controlador CAN! Verifique as conexões.");
    while (1) delay(100);
  }

  // Conexão Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("WiFi connected!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  // Configuração WebSocket
  webSocket.begin(serverAddress, serverPort, "/");
  webSocket.onEvent(webSocketEvent);

  // Cria tasks
  xTaskCreate(canTask, "CAN Task", 4096, NULL, 2, NULL);
  xTaskCreate(webSocketTask, "WebSocket Task", 4096, NULL, 1, NULL);

  Serial.println("------ Setup completo - Tasks rodando ------");
}

void loop() {
  // Loop principal vazio - as tarefas fazem o trabalho
  vTaskDelay(1000 / portTICK_PERIOD_MS);
}