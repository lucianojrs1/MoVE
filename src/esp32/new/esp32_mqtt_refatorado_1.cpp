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
#include "../../config/constants.h"
// ------------------------------------------------------------------
// --- CONFIGURAÇÕES ---
// ------------------------------------------------------------------
#define CAN_TX_PIN 2
#define CAN_RX_PIN 15
#define ledCAN 16
#define ledMQTT 17

#define TESTMODE true  // Se true, gera dados aleatórios para teste sem hardware CAN
#define DEBUGMODE false
#define BufferSize 250  // Buffer aumentado para evitar perda em latências de rede

const char *ssid = "Salvacao_2_conto";
const char *password = "mimda2conto";
const char *mqtt_server = "192.168.1.185";
const char* MQTT_TOPIC = "moto/telemetria";
const int mqtt_port = 31125;

const TwaiSpeed CAN_SPEED = TWAI_SPEED_250KBPS;

// Configurações do Fuso Horário (Brasil - Pernambuco)
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -3 * 3600; 
const int daylightOffset_sec = 0;      

// Intervalo que a Task MQTT acorda para limpar a fila
const TickType_t TRANSMIT_INTERVAL = pdMS_TO_TICKS(50);

// ------------------------------------------------------------------
// --- ESTRUTURAS E VARIÁVEIS GLOBAIS ---
// ------------------------------------------------------------------

struct CanMessage {
  uint32_t id;
  uint8_t data[8];
  uint8_t length;
  bool isExtended;
  int64_t timestamp; // Armazena o momento exato da leitura
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
    String clientId = "ESP32-Voltz-";
    clientId += String(random(0xffff), HEX);
    
    if (client.connect(clientId.c_str())) {
      Serial.println("Conectado!");
    } else {
      Serial.print("falha, rc=");
      Serial.print(client.state());
      Serial.println(" tentando novamente em 2s");
      vTaskDelay(pdMS_TO_TICKS(2000));
    }
  }
}

// ------------------------------------------------------------------
// --- TAREFAS (FREERTOS) ---
// ------------------------------------------------------------------

// 1. Task Core 0: Leitura de Alta Velocidade e Timestamper
void canSourceTask(void* pvParameters) {
  for (;;) {
    CanMessage frame;
    bool hasData = false;

    if (TESTMODE) {
      // Simulação de tráfego para teste
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
      
      // Timestamp da simulação
      struct timeval tv_now;
      gettimeofday(&tv_now, NULL);
      frame.timestamp = (int64_t)tv_now.tv_sec * 1000LL + (tv_now.tv_usec / 1000LL);
      
      hasData = true;
      vTaskDelay(pdMS_TO_TICKS(20)); 
    } else {
      CanFrame rx;
      // readFrame(rx, 10) espera até 10ms por um frame no buffer do driver
      if (ESP32Can.readFrame(rx, 10)) {
        digitalWrite(ledCAN, !digitalRead(ledCAN)); 
        
        // CAPTURA DO TIMESTAMP NO MOMENTO DA CHEGADA
        struct timeval tv_now;
        gettimeofday(&tv_now, NULL);
        frame.timestamp = (int64_t)tv_now.tv_sec * 1000LL + (tv_now.tv_usec / 1000LL);

        frame.id = rx.identifier;
        frame.length = rx.data_length_code;
        frame.isExtended = rx.extd;
        memcpy(frame.data, rx.data, rx.data_length_code);
        hasData = true;
      }
    }

    if (hasData) {
      // Envia para a fila para processamento no Core 1
      if (xQueueSend(canRawQueue, &frame, 0) != pdTRUE) {
        if (DEBUGMODE) Serial.println("Fila de processamento cheia!");
      }
    }
    vTaskDelay(0); // Cede tempo para o IDLE do Core 0
  }
}

// 2. Task Core 1: Gestão Wi-Fi e Publicação MQTT em Lote
void mqttPublisherTask(void* pvParameters) {
  CanMessage rawFrame;
  char jsonBuffer[256];
  TickType_t xLastWakeTime = xTaskGetTickCount();

  client.setServer(mqtt_server, mqtt_port);

  for (;;) {
    // Manutenção da Conexão WiFi
    if (WiFi.status() != WL_CONNECTED) {
      WiFi.begin(ssid, password);
      int timeout = 0;
      while (WiFi.status() != WL_CONNECTED && timeout < 10) {
        vTaskDelay(pdMS_TO_TICKS(500));
        timeout++;
      }
    }

    // Manutenção da Conexão MQTT
    if (WiFi.status() == WL_CONNECTED && !client.connected()) {
      reconnectMQTT();
    }
    client.loop();

    // PROCESSAMENTO EM LOTE: Esvazia toda a fila acumulada
    while (xQueueReceive(canRawQueue, &rawFrame, 0) == pdTRUE) {
      digitalWrite(ledMQTT, HIGH);

      StaticJsonDocument<256> doc;
      doc["canId"] = rawFrame.id;
      doc["ide"] = rawFrame.isExtended;

      // Conversão eficiente de dados para Hex String
      char dataHex[25]; 
      char* ptr = dataHex;
      for (int i = 0; i < rawFrame.length; i++) {
        ptr += sprintf(ptr, i == 0 ? "%02X" : " %02X", rawFrame.data[i]);
      }
      doc["data"] = dataHex;
      doc["dlc"] = rawFrame.length;
      
      // ENVIO DO TIMESTAMP ORIGINAL (Capturado na Task CAN)
      doc["ts"] = rawFrame.timestamp; 

      serializeJson(doc, jsonBuffer, sizeof(jsonBuffer));

      if (client.connected()) {
        client.publish(MQTT_TOPIC, jsonBuffer);
      }
      
      digitalWrite(ledMQTT, LOW);
      vTaskDelay(0); // Evita bloqueio da stack Wi-Fi
    }

    // Aguarda até o próximo ciclo de transmissão
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

  // Criação da fila de mensagens
  canRawQueue = xQueueCreate(BufferSize, sizeof(CanMessage));

  // Início do WiFi
  WiFi.begin(ssid, password);
  
  // Configuração do NTP para sincronizar o timestamp real
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  // Inicialização do Driver CAN
  ESP32Can.setPins(CAN_TX_PIN, CAN_RX_PIN);
  if (!TESTMODE) {
    if (!ESP32Can.begin(CAN_SPEED)) {
      Serial.println("Critico: Falha ao iniciar barramento CAN");
      while (1) delay(1000);
    }
  }

  // Task de leitura no Core 0 (Prioridade 3 - Máxima para dados)
  xTaskCreatePinnedToCore(canSourceTask, "CAN_Source", 4096, NULL, 3, NULL, 0); 
  
  // Task de Wi-Fi/MQTT no Core 1 (Prioridade 1 - Menor)
  xTaskCreatePinnedToCore(mqttPublisherTask, "MQTT_Pub", 8192, NULL, 1, NULL, 1); 
}

void loop() {
  // Deleta o loop padrão para economizar recursos, o sistema roda nas Tasks
  vTaskDelete(NULL); 
}