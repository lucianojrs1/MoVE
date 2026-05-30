// ------------------------------------------------------------------
// --- BIBLIOTECAS ---
// ------------------------------------------------------------------
#include <ESP32-TWAI-CAN.hpp>  // ESP32-TWAI-CAN by sorek.uk
#include <PubSubClient.h>      // MQTT
#include <WiFi.h>              // WiFi
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <ArduinoJson.h>  // Para gerar JSON facilmente
#include "time.h"
#include "../../config/constants.h"

// ------------------------------------------------------------------
// --- CONFIGURAÇÕES ---
// ------------------------------------------------------------------
#define CAN_TX_PIN 2
#define CAN_RX_PIN 15
#define TESTMODE true  // true = Simula, false = Lê CAN real
#define DEBUGMODE false
#define BufferSize 500

const char* ssid = "Salvacao_2_conto";
const char* password = "mimda2conto";
const char* mqtt_server = "broker.hivemq.com";
const char* MQTT_TOPIC = "moto/telemetria";
const int mqtt_port = 1883;
struct timeval tv;

// Configurações do Fuso Horário (Ex: Brasília é -3h)
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -3 * 3600;  // -3 horas em segundos
const int daylightOffset_sec = 0;      // Horário de verão (0 se não houver)

// ------------------------------------------------------------------
// --- ESTRUTURAS E VARIÁVEIS GLOBAIS ---
// ------------------------------------------------------------------

// Estrutura única para trafegar o frame bruto
struct CanMessage {
  uint32_t id;
  uint8_t data[8];
  uint8_t length;
  bool isExtended;
};

// Objetos MQTT
WiFiClient espClient;
PubSubClient client(espClient);

// Fila única para comunicação entre CAN e MQTT
QueueHandle_t canRawQueue;

// ------------------------------------------------------------------
// --- FUNÇÕES AUXILIARES ---
// ------------------------------------------------------------------

void reconnectMQTT() {
  while (!client.connected()) {
    String clientId = "ESP32-Raw-";
    clientId += String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      Serial.println("MQTT Conectado");
    } else {
      delay(2000);
    }
  }
}

// ------------------------------------------------------------------
// --- TAREFAS ---
// ------------------------------------------------------------------

// 1. Task de Leitura/Simulação CAN
void canSourceTask(void* pvParameters) {
  const unsigned long SIM_INTERVAL_MS = 100;  // Intervalo da simulação

  while (true) {
    CanMessage frame;

    if (TESTMODE) {
      // --- MODO SIMULAÇÃO ---
      frame.id = (random(1, 100) < 70)
                   ? (random(0, 2) == 0 ? BASE_BATTERY_ID : BASE_CONTROLLER_ID)
                   : random(0x000, 0x7FF + 1);
      frame.length = 8;
      frame.isExtended = false;
      for (int i = 0; i < 8; i++) {
        if (i == 5) {
          int choice = random(0, 3);  
          switch (choice) {
            case 0:
              frame.data[i] = 0x45;
              break;
            case 1:
              frame.data[i] = 0x4D;
              break;
            case 2:
              frame.data[i] = 0x55;
              break;
          }
        } else {
          frame.data[i] = random(0, 255);
        }
      }
    } else {
      // --- MODO REAL ---
      twai_message_t rx;
      if (ESP32Can.readFrame(&rx)) {
        frame.id = rx.identifier;
        frame.length = rx.data_length_code;
        frame.isExtended = (rx.flags & TWAI_MSG_FLAG_EXTD) != 0;
        memcpy(frame.data, rx.data, rx.data_length_code);
      } else {
        vTaskDelay(1 / portTICK_PERIOD_MS);  // Aguarda se não houver dado
        continue;
      }
    }

    // Envia frame bruto para a fila (timeout de 100ms)
    if (xQueueSend(canRawQueue, &frame, pdMS_TO_TICKS(100)) != pdTRUE) {
      Serial.println("Fila cheia! Frame descartado.");
    }

    if (TESTMODE) vTaskDelay(SIM_INTERVAL_MS / portTICK_PERIOD_MS);
  }
}

// 2. Task de Publicação MQTT (Consome dados brutos)
void mqttPublisherTask(void* pvParameters) {
  CanMessage rawFrame;
  char jsonBuffer[256];

  // Configura servidor MQTT
  client.setServer(mqtt_server, mqtt_port);

  while (true) {
    // Garante conexão MQTT
    if (!client.connected()) {
      reconnectMQTT();
    }
    client.loop();

    // Recebe frame bruto da fila (bloqueia até chegar dado)
    if (xQueueReceive(canRawQueue, &rawFrame, portMAX_DELAY) == pdTRUE) {

      // Monta JSON com dados brutos (Hexadecimal para facilitar leitura de CAN)
      StaticJsonDocument<256> doc;
      doc["canId"] = rawFrame.id;
      doc["ide"] = rawFrame.isExtended;

      // Converte array de bytes para string Hex "AA BB CC..."
      String dataHex = "";
      for (int i = 0; i < rawFrame.length; i++) {
        if (i > 0) dataHex += " ";
        char hex[3];
        sprintf(hex, "%02X", rawFrame.data[i]);
        dataHex += hex;
      }
      doc["data"] = dataHex;
      doc["dlc"] = rawFrame.length;

      struct tm timeinfo;
      if (!getLocalTime(&timeinfo)) {
        Serial.println("Falha ao obter a hora");
        doc["ts"] = millis();
      } else {
        // Mostra a data formatada no Serial
        //Serial.println(&timeinfo, "%d/%m/%Y %H:%M:%S");

        // Para enviar ao JS, você pegaria o timestamp:
        time_t agora;
        time(&agora);
        //Serial.printf("Timestamp para o JS: %ld\n", agora);

        doc["ts"] = agora;
      }
      // Serializa para string
      serializeJson(doc, jsonBuffer, sizeof(jsonBuffer));

      // Publica no tópico
      if (client.publish(MQTT_TOPIC, jsonBuffer)) {
        // Sucesso (opcional: piscar LED)
      } else {
        Serial.println("Falha no publish MQTT");
      }
    }
  }
}

// ------------------------------------------------------------------
// --- SETUP E LOOP ---
// ------------------------------------------------------------------
void setup() {
  Serial.begin(115200);

  // Inicializa Fila
  canRawQueue = xQueueCreate(BufferSize, sizeof(CanMessage));  // Buffer para 10 frames

  // Inicializa CAN
  ESP32Can.setPins(CAN_TX_PIN, CAN_RX_PIN);
  if (!TESTMODE) {
    if (ESP32Can.begin(TWAI_SPEED_250KBPS)) {
      Serial.println("CAN Real Iniciado");
    } else {
      Serial.println("Erro ao iniciar CAN");
      while (1) delay(1000);
    }
  } else {
    Serial.println("Modo Simulação Ativo");
  }

  // Inicializa WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Conectado");
  // Inicia a sincronização com o servidor NTP para horario
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  // Cria Tasks
  // Prioridade 1 para ambas, rodando em núcleos diferentes se desejar (affinity NULL)
  xTaskCreate(canSourceTask, "CAN_Source", 4096, NULL, 1, NULL);
  xTaskCreate(mqttPublisherTask, "MQTT_Pub", 8192, NULL, 1, NULL);
}

void loop() {
  // Nada aqui, FreeRTOS gerencia tudo
  vTaskDelay(1000 / portTICK_PERIOD_MS);
}