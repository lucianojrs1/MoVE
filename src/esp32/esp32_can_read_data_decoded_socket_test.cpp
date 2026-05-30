#include <ArduinoJson.h>
#include <ESP32-TWAI-CAN.hpp>
#include <WebSocketsClient.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <string.h>
#include "../../config/constants.h"

#define testMode true
// ------------------------------------------------------------------
// --- CONFIGURAÇÃO DE PINOS E VELOCIDADE ---
// ------------------------------------------------------------------
#define CAN_TX_PIN 5
#define CAN_RX_PIN 4
const TwaiSpeed CAN_SPEED = TWAI_SPEED_250KBPS;


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

// Mutex para proteger acesso concorrente às variáveis de dados
SemaphoreHandle_t dataMutex;

const char *ssid = "Salvacao_2_conto";
const char *password = "mimda2conto";
const char *serverAddress = "192.168.1.160";
const uint16_t serverPort = 3001;

WebSocketsClient webSocket;

// Variáveis para armazenar os dados anteriores
BatteryData batteryPrev = {};
MotorControllerData motorControllerPrev = {};

// Variáveis para simulação
bool dadosCanRecebidos = false;
unsigned long ultimoDadoCanRecebido = 0;
const unsigned long TIMEOUT_CAN = 5000; // 5 segundos sem dados = simular

// Função para gerar frames CAN simulados
void gerarFrameCanSimulado(twai_message_t &frame) {
  // Gera um ID aleatório (standard frame)
  // Pode ser um dos esperados ou outro aleatório
  uint32_t randomId = random(0x000, 0x7FF + 1); // IDs de 0x000 a 0x7FF

  // Escolhe aleatoriamente se será o ID esperado ou aleatório
  if (random(0, 100) < 70) { // 70% de chance de ser um ID esperado
    frame.identifier = (random(0, 2) == 0) ? BASE_BATTERY_ID : BASE_CONTROLLER_ID;
  } else {
    frame.identifier = randomId; // ID aleatório
  }

  frame.flags = 0; // Standard frame
  frame.data_length_code = random(1, 9); // Tamanho entre 1 e 8 bytes

  // Preenche os dados com valores aleatórios
  for (int i = 0; i < frame.data_length_code; i++) {
    frame.data[i] = random(0, 256); // Bytes entre 0 e 255
  }

  //Serial.printf("[Simulacao] Gerado frame CAN ID: 0x%X com %d bytes\n", frame.identifier, frame.data_length_code);
}

// ------------------------------------------------------------------
// --- FUNÇÕES DE DECODIFICAÇÃO ---
// ------------------------------------------------------------------
void decodeBatteryData(byte *data) {
  battery.current = (int)((data[2] * 256 + data[3]) * 0.1);
  battery.voltage = (int)((data[0] * 256 + data[1]) * 0.1);
  battery.soc = (int)data[6];
  battery.soh = (int)data[7];
  battery.temperature = (int)data[4];
  battery.valid = true;
}

void decodeMotorControllerData(byte *data) {
  motorController.motorSpeedRpm = (int)(data[0] * 256 + data[1]);
  motorController.motorTorque = (float)((data[2] * 256 + data[3]) * 0.1);
  motorController.motorTemperature = (int)(data[7] - 40);
  motorController.controllerTemperature = (int)(data[6] - 40);
  motorController.valid = true;
}

// ------------------------------------------------------------------
// --- TAREFA PARA LEITURA CAN (REAL OU SIMULADA) ---
// ------------------------------------------------------------------
void canTask(void *pvParameters) {
  twai_message_t rxFrame;

  while (true) {
    // Tenta ler frame real
    bool frameLido = ESP32Can.readFrame(&rxFrame);

    // Se não leu frame real e está no modo simulação
    if (!frameLido && testMode ) {
      if (millis() - ultimoDadoCanRecebido > TIMEOUT_CAN) {
        gerarFrameCanSimulado(rxFrame);
        frameLido = true;
        Serial.println("[Simulacao] Gerando frame CAN simulado.");
      }
    }

    if (frameLido) {
      // Atualiza timestamp
      ultimoDadoCanRecebido = millis();
      dadosCanRecebidos = true;

      // Verifica SE NÃO É um frame estendido
      if (!(rxFrame.flags & TWAI_MSG_FLAG_EXTD)) {
        uint32_t std_id = rxFrame.identifier & 0x7FF;

        if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
          if (std_id == BASE_BATTERY_ID) {
            BatteryData tempBattery;
            tempBattery.current = (int)((rxFrame.data[2] * 256 + rxFrame.data[3]) * 0.1);
            tempBattery.voltage = (int)((rxFrame.data[0] * 256 + rxFrame.data[1]) * 0.1);
            tempBattery.soc = (int)rxFrame.data[6];
            tempBattery.soh = (int)rxFrame.data[7];
            tempBattery.temperature = (int)rxFrame.data[4];
            tempBattery.valid = true;

            bool dadosAtualizados = false;
            String mudancas = "Dados da bateria mudaram: ";

            if (tempBattery.current != batteryPrev.current) {
              mudancas += "Corrente(" + String(batteryPrev.current) + " -> " + String(tempBattery.current) + ") ";
              dadosAtualizados = true;
            }
            if (tempBattery.voltage != batteryPrev.voltage) {
              mudancas += "Voltagem(" + String(batteryPrev.voltage) + " -> " + String(tempBattery.voltage) + ") ";
              dadosAtualizados = true;
            }
            if (tempBattery.soc != batteryPrev.soc) {
              mudancas += "SoC(" + String(batteryPrev.soc) + " -> " + String(tempBattery.soc) + ") ";
              dadosAtualizados = true;
            }
            if (tempBattery.soh != batteryPrev.soh) {
              mudancas += "SoH(" + String(batteryPrev.soh) + " -> " + String(tempBattery.soh) + ") ";
              dadosAtualizados = true;
            }
            if (tempBattery.temperature != batteryPrev.temperature) {
              mudancas += "Temperatura(" + String(batteryPrev.temperature) + " -> " + String(tempBattery.temperature) + ") ";
              dadosAtualizados = true;
            }

            if (dadosAtualizados) {
              battery = tempBattery;
              batteryPrev = tempBattery;
              Serial.println(mudancas);
            }
          } else if (std_id == BASE_CONTROLLER_ID) {
            MotorControllerData tempMotorController;
            tempMotorController.motorSpeedRpm = (int)(rxFrame.data[0] * 256 + rxFrame.data[1]);
            tempMotorController.motorTorque = (float)((rxFrame.data[2] * 256 + rxFrame.data[3]) * 0.1);
            tempMotorController.motorTemperature = (int)(rxFrame.data[7] - 40);
            tempMotorController.controllerTemperature = (int)(rxFrame.data[6] - 40);
            tempMotorController.valid = true;

            bool dadosAtualizados = false;
            String mudancas = "Dados do motor/controlador mudaram: ";

            if (tempMotorController.motorSpeedRpm != motorControllerPrev.motorSpeedRpm) {
              mudancas += "RPM(" + String(motorControllerPrev.motorSpeedRpm) + " -> " + String(tempMotorController.motorSpeedRpm) + ") ";
              dadosAtualizados = true;
            }
            if (tempMotorController.motorTorque != motorControllerPrev.motorTorque) {
              mudancas += "Torque(" + String(motorControllerPrev.motorTorque) + " -> " + String(tempMotorController.motorTorque) + ") ";
              dadosAtualizados = true;
            }
            if (tempMotorController.motorTemperature != motorControllerPrev.motorTemperature) {
              mudancas += "Temp.Motor(" + String(motorControllerPrev.motorTemperature) + " -> " + String(tempMotorController.motorTemperature) + ") ";
              dadosAtualizados = true;
            }
            if (tempMotorController.controllerTemperature != motorControllerPrev.controllerTemperature) {
              mudancas += "Temp.Controlador(" + String(motorControllerPrev.controllerTemperature) + " -> " + String(tempMotorController.controllerTemperature) + ") ";
              dadosAtualizados = true;
            }

            if (dadosAtualizados) {
              motorController = tempMotorController;
              motorControllerPrev = tempMotorController;
              Serial.println(mudancas);
            }
          }
          xSemaphoreGive(dataMutex);
        }
      }
    }
    vTaskDelay(1 / portTICK_PERIOD_MS); // 1ms delay
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
    Serial.print("[WSc] Received: ");
    Serial.println((char *)payload);
    break;
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  dataMutex = xSemaphoreCreateMutex();
  if (dataMutex == NULL) {
    Serial.println("ERRO: Falha ao criar mutex!");
    return;
  }

  ESP32Can.setPins(CAN_TX_PIN, CAN_RX_PIN);
  if (ESP32Can.begin(CAN_SPEED)) {
    Serial.println("Controlador CAN (TWAI) iniciado com sucesso!");
    Serial.println("Monitorando em 250 kbps nos pinos TX:5 e RX:4...");
  } else {
    Serial.println("ERRO: Falha ao iniciar o controlador CAN! Verifique as conexões.");
    while (1) delay(100);
  }

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("WiFi connected!");

  webSocket.begin(serverAddress, serverPort, "/");
  webSocket.onEvent(webSocketEvent);

  xTaskCreate(canTask, "CAN Task", 4096, NULL, 2, NULL);
  Serial.println("Tasks criadas com sucesso!");
}

void loop() {
  webSocket.loop();

  static unsigned long lastSend = 0;
  if (millis() - lastSend > 2000) {
    if (webSocket.isConnected()) {
      StaticJsonDocument<512> doc;

      if (xSemaphoreTake(dataMutex, 100 / portTICK_PERIOD_MS) == pdTRUE) {
        if (battery.valid) {
          doc["battery"]["current"] = battery.current;
          doc["battery"]["voltage"] = battery.voltage;
          doc["battery"]["soc"] = battery.soc;
          doc["battery"]["soh"] = battery.soh;
          doc["battery"]["temperature"] = battery.temperature;
        }
        if (motorController.valid) {
          doc["motorController"]["motorSpeedRpm"] = motorController.motorSpeedRpm;
          doc["motorController"]["motorTorque"] = motorController.motorTorque;
          doc["motorController"]["motorTemperature"] = motorController.motorTemperature;
          doc["motorController"]["controllerTemperature"] = motorController.controllerTemperature;
        }
        doc["status"] = dadosCanRecebidos ? "dados_reais" : "dados_simulados";
        xSemaphoreGive(dataMutex);

        String jsonString;
        serializeJson(doc, jsonString);
        webSocket.sendTXT(jsonString);
        Serial.println("Dados CAN enviados via WebSocket:");
        Serial.println(jsonString);
      }
    }
    lastSend = millis();
  }
}