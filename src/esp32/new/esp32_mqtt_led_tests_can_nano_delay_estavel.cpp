// ------------------------------------------------------------------
// --- BIBLIOTECAS ---
// ------------------------------------------------------------------
#include <ESP32-TWAI-CAN.hpp> 
#include <PubSubClient.h>      
#include <WiFi.h>              
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <ArduinoJson.h>  
#include "time.h"

// ------------------------------------------------------------------
// --- CONFIGURAÇÕES ---
// ------------------------------------------------------------------
#define CAN_TX_PIN 2
#define CAN_RX_PIN 15
#define ledCAN 16
#define ledMQTT 17

#define TESTMODE false  
#define DEBUGMODE false
#define BufferSize 50  

const char* ssid = "Voltz";
const char* password = "12345678";
const char* mqtt_server = "192.168.43.117";
const char* MQTT_TOPIC = "moto/telemetria";
const int mqtt_port = 1883;

const TwaiSpeed CAN_SPEED = TWAI_SPEED_250KBPS;

// Configurações do Fuso Horário
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -3 * 3600; 
const int daylightOffset_sec = 0;      

// Intervalo de transmissão MQTT (Ex: 100ms)
const TickType_t TRANSMIT_INTERVAL = pdMS_TO_TICKS(200);

// ------------------------------------------------------------------
// --- ESTRUTURAS E VARIÁVEIS GLOBAIS ---
// ------------------------------------------------------------------

struct CanMessage {
  uint32_t id;
  uint8_t data[8];
  uint8_t length;
  bool isExtended;
};

WiFiClient espClient;
PubSubClient client(espClient);
QueueHandle_t canRawQueue;

// ------------------------------------------------------------------
// --- FUNÇÕES AUXILIARES ---
// ------------------------------------------------------------------

void reconnectMQTT() {
  if (WiFi.status() != WL_CONNECTED) return;

  while (!client.connected()) {
    Serial.print("Tentando conectar MQTT...");
    String clientId = "ESP32-Raw-";
    clientId += String(random(0xffff), HEX);
    
    if (client.connect(clientId.c_str())) {
      Serial.println("Conectado");
    } else {
      Serial.print("falha, rc=");
      Serial.print(client.state());
      Serial.println(" tentando novamente em 2s");
      vTaskDelay(pdMS_TO_TICKS(2000));
    }
  }
}

// ------------------------------------------------------------------
// --- TAREFAS ---
// ------------------------------------------------------------------

// 1. Task Core 0: Leitura/Simulação CAN
void canSourceTask(void* pvParameters) {
  for (;;) {
    CanMessage frame;
    bool hasData = false;

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
      hasData = true;
      vTaskDelay(pdMS_TO_TICKS(20)); // Simula chegada de dados rápida
      
    } else {
      CanFrame rx;
      if (ESP32Can.readFrame(rx, 0)) {
        digitalWrite(ledCAN, !digitalRead(ledCAN)); 
        frame.id = rx.identifier;
        frame.length = rx.data_length_code;
        frame.isExtended = rx.extd;
        memcpy(frame.data, rx.data, rx.data_length_code);
        hasData = true;
      }
    }

    if (hasData) {
      if (xQueueSend(canRawQueue, &frame, 0) != pdTRUE) {
        if (DEBUGMODE) Serial.println("Fila cheia!");
      }
    }
    vTaskDelay(pdMS_TO_TICKS(1)); 
  }
}

// 2. Task Core 1: Gestão Wi-Fi e Publicação MQTT com Delay Fixo
void mqttPublisherTask(void* pvParameters) {
  CanMessage rawFrame;
  char jsonBuffer[256];
  struct timeval tv;
  
  // Para controle de tempo fixo
  TickType_t xLastWakeTime = xTaskGetTickCount();

  client.setServer(mqtt_server, mqtt_port);

  for (;;) {
    // Gestão de Conexão Wi-Fi
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi OFF. Reconectando...");
      WiFi.disconnect();
      WiFi.begin(ssid, password);
      // Aguarda conexão sem travar o processador
      int timeout = 0;
      while (WiFi.status() != WL_CONNECTED && timeout < 20) {
        vTaskDelay(pdMS_TO_TICKS(500));
        timeout++;
      }
    }

    // Gestão de Conexão MQTT
    if (WiFi.status() == WL_CONNECTED && !client.connected()) {
      reconnectMQTT();
    }
    client.loop();

    // Processa a fila e envia
    if (xQueueReceive(canRawQueue, &rawFrame, 0) == pdTRUE) {
      digitalWrite(ledMQTT, HIGH);

      StaticJsonDocument<256> doc;
      doc["canId"] = rawFrame.id;
      doc["ide"] = rawFrame.isExtended;

      String dataHex = "";
      for (int i = 0; i < rawFrame.length; i++) {
        if (i > 0) dataHex += " ";
        char hex[3];
        sprintf(hex, "%02X", rawFrame.data[i]);
        dataHex += hex;
      }
      doc["data"] = dataHex;
      doc["dlc"] = rawFrame.length;

      gettimeofday(&tv, NULL);
      int64_t time_ms = (int64_t)tv.tv_sec * 1000LL + (tv.tv_usec / 1000LL);
      doc["ts"] = time_ms;

      serializeJson(doc, jsonBuffer, sizeof(jsonBuffer));

      if (client.connected()) {
        client.publish(MQTT_TOPIC, jsonBuffer);
      }
      
      digitalWrite(ledMQTT, LOW);
    }

    // Garante que o loop da Task aconteça sempre no intervalo definido
    vTaskDelayUntil(&xLastWakeTime, TRANSMIT_INTERVAL);
  }
}

// ------------------------------------------------------------------
// --- SETUP E LOOP ---
// ------------------------------------------------------------------
void setup() {
  Serial.begin(115200);

  pinMode(ledCAN, OUTPUT);
  pinMode(ledMQTT, OUTPUT);

  canRawQueue = xQueueCreate(BufferSize, sizeof(CanMessage));

  WiFi.begin(ssid, password);
  Serial.print("Iniciando WiFi");
  
  // Configura NTP assíncrono
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  ESP32Can.setPins(CAN_TX_PIN, CAN_RX_PIN);
  if (!TESTMODE) {
    if (!ESP32Can.begin(CAN_SPEED)) {
      Serial.println("Erro CAN");
      while (1) delay(1000);
    }
  }

  // Criação das Tasks nos núcleos específicos
  xTaskCreatePinnedToCore(canSourceTask, "CAN_Source", 4096, NULL, 2, NULL, 0); 
  xTaskCreatePinnedToCore(mqttPublisherTask, "MQTT_Pub", 8192, NULL, 1, NULL, 1); 
}

void loop() {
  // Deleta a task do loop para economizar recursos, 
  // já que tudo roda via FreeRTOS
  vTaskDelete(NULL); 
}
