// -----------------------------
// Bibliotecas
// -----------------------------
#include <ArduinoJson.h>
#include <ESP32-TWAI-CAN.hpp>
#include <HTTPClient.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <string.h>
#include <stdarg.h>
#include "../config/constants.h"


// -----------------------------
// Configurações
// -----------------------------
#define TESTMODE false
#define DEBUGMODE false

// CAN
#define CAN_TX_PIN 5
#define CAN_RX_PIN 4
const TwaiSpeed CAN_SPEED = TWAI_SPEED_250KBPS;

// Estrutura de mensagem CAN
struct CanMessage {
  uint32_t id;
  uint8_t data[8];
  uint8_t length;
  bool isExtended;
};

// Filas e buffers
#define CAN_QUEUE_SIZE 500
#define HTTP_SEND_THRESHOLD 250
#define HTTP_SEND_INTERVAL_MS 2000  // 2s fallback

// Wi-Fi
const char* ssid = "CINGUESTS";
const char* password = "acessocin";

// URLs do backend (ATUALIZE COM O IP DO SEU SERVIDOR NO KILLERCODE!)
const char* DEVICE_REGISTER_URL = "https://a8c690a76502-10-244-10-44-31602.saci.r.killercoda.com/api/device";
const char* TELEMETRY_URL = "https://a8c690a76502-10-244-10-44-31602.saci.r.killercoda.com/api/can/";

// -----------------------------
// Variáveis globais
// -----------------------------
QueueHandle_t canFrameQueue;
QueueHandle_t logMessageQueue;
#define MAX_LOG_MESSAGE_LEN 128

String deviceId = "";
bool dispositivoRegistrado = false;


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
// -----------------------------
// Função de log segura
// -----------------------------
void logMessage(const char* fmt, ...) {
  char buffer[MAX_LOG_MESSAGE_LEN];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buffer, sizeof(buffer), fmt, args);
  va_end(args);
  xQueueSendToBack(logMessageQueue, buffer, 0);
}
// -----------------------------
// Cadastro do Dispositivo
// -----------------------------
bool cadastrarDispositivo() {
  if (dispositivoRegistrado) return true;

  HTTPClient http;
  http.setTimeout(10000);

  // 1. Preparar URL
  if (!http.begin(DEVICE_REGISTER_URL)) {
    logMessage("❌ Falha ao iniciar HTTP para cadastro");
    return false;
  }

  // 2. Criar JSON com ArduinoJson
  StaticJsonDocument<300> doc;
  doc["location"]["type"] = "Point";
  JsonArray coords = doc["location"]["coordinates"].to<JsonArray>();
  coords.add(-46.5755);  // longitude
  coords.add(-23.6789);  // latitude

  String jsonString;
  serializeJson(doc, jsonString);

  // 3. Enviar requisição
  http.addHeader("Content-Type", "application/json");
  int httpCode = http.POST(jsonString);

  // 4. Tratar resposta
  if (httpCode == 200 || httpCode == 201) {
    String payload = http.getString();
    // Analisar resposta com ArduinoJson
    DynamicJsonDocument respDoc(512);
    DeserializationError error = deserializeJson(respDoc, payload);

    if (!error) {
      // Tenta extrair deviceId de response.data.deviceId OU response.savedData.deviceId
      String receivedId = "";
      if (respDoc.containsKey("deviceId")) {
        receivedId = respDoc["deviceId"].as<String>();
      } else if (respDoc.containsKey("savedData") && respDoc["savedData"].containsKey("deviceId")) {
        receivedId = respDoc["savedData"]["deviceId"].as<String>();
      }

      if (receivedId.length() > 0) {
        deviceId = receivedId;
        dispositivoRegistrado = true;
        logMessage("✅ deviceId recebido: %s", deviceId.c_str());
        http.end();
        return true;
      } else {
        logMessage("❌ deviceId não encontrado na resposta");
      }
    } else {
      logMessage("❌ Erro ao analisar JSON da resposta: %s", error.c_str());
    }
  } else {
    logMessage("❌ Erro HTTP no cadastro: %d", httpCode);
  }

  http.end();
  return false;
}
// -----------------------------
// Tasks
// -----------------------------
void serialLoggerTask(void* pv) {
  char buf[MAX_LOG_MESSAGE_LEN];
  static char last[MAX_LOG_MESSAGE_LEN] = { 0 };
  while (1) {
    if (xQueueReceive(logMessageQueue, buf, portMAX_DELAY)) {
      if (strcmp(buf, last) != 0) {
        Serial.println(buf);
        strcpy(last, buf);
      }
    }
    vTaskDelay(1);
  }
}

void debugTask(void* pvParameters) {
  const unsigned long DEBUG_INTERVAL_MS = 1000;  // 1 segundos
  while (true) {
    // --- Status da Fila CAN ---

    if (canFrameQueue != NULL) {
      UBaseType_t queueItems = uxQueueMessagesWaiting(canFrameQueue);
      UBaseType_t queueSpaces = uxQueueSpacesAvailable(canFrameQueue);
      UBaseType_t queueLength = CAN_QUEUE_SIZE;
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

void canSimTask(void* pv) {
  const unsigned long SIM_INTERVAL_MS = 50;  // 20 Hz
  while (1) {
    CanMessage frame;
    frame.id = (random(0, 100) < 70)
                 ? (random(0, 2) == 0 ? BASE_BATTERY_ID : BASE_CONTROLLER_ID)
                 : random(0x001, 0x7FF + 1);
    frame.length = 8;
    frame.isExtended = false;
    for (int i = 0; i < 8; i++) frame.data[i] = random(0, 256);

    if (xQueueSend(canFrameQueue, &frame, 0) != pdTRUE) {
      logMessage("⚠️ Fila cheia! Frame simulado descartado");
    }
    vTaskDelay(SIM_INTERVAL_MS / portTICK_PERIOD_MS);
  }
}

void canTask(void* pv) {
  twai_message_t rx;
  while (1) {
    if (ESP32Can.readFrame(&rx)) {
      CanMessage frame;
      frame.id = rx.identifier;
      frame.length = rx.data_length_code;
      frame.isExtended = (rx.flags & TWAI_MSG_FLAG_EXTD) != 0;

      memcpy(frame.data, rx.data, rx.data_length_code);

      if (frame.id == BASE_BATTERY_ID) {
        // Decodifica os dados recebidos em uma variável temporária
        BatteryData tempBattery;
        tempBattery.current = (int)((frame.data[2] * 256 + frame.data[3]) * 0.1);
        tempBattery.voltage = (int)((frame.data[0] * 256 + frame.data[1]) * 0.1);
        tempBattery.soc = (int)frame.data[6];
        tempBattery.soh = (int)frame.data[7];
        tempBattery.temperature = (int)frame.data[4];
        tempBattery.valid = true;

        bool dadosAtualizados = false;                   // Flag para saber se houve alguma mudança
        String mudancas = "Dados da bateria mudaram: ";  // String para acumular as mudanças

        // Compara campo a campo e adiciona à string de mudanças se for diferente
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
          // Se houve mudança, atualiza os dados globais e os anteriores
          battery = tempBattery;
          batteryPrev = tempBattery;
          Serial.println(mudancas);  // Imprime a string com as mudanças detalhadas
        } else {
          //Serial.println("Dados da bateria recebidos, mas NÃO mudaram.");
        }
      } else if (frame.id == BASE_CONTROLLER_ID) {
        // Decodifica os dados recebidos em uma variável temporária
        MotorControllerData tempMotorController;
        tempMotorController.motorSpeedRpm = (int)(frame.data[0] * 256 + frame.data[1]);
        tempMotorController.motorTorque = (float)((frame.data[2] * 256 + frame.data[3]) * 0.1);
        tempMotorController.motorTemperature = (int)(frame.data[7] - 40);
        tempMotorController.controllerTemperature = (int)(frame.data[6] - 40);
        tempMotorController.valid = true;

        bool dadosAtualizados = false;                             // Flag para saber se houve alguma mudança
        String mudancas = "Dados do motor/controlador mudaram: ";  // String para acumular as mudanças

        // Compara campo a campo e adiciona à string de mudanças se for diferente
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
          // Se houve mudança, atualiza os dados globais e os anteriores
          motorController = tempMotorController;
          motorControllerPrev = tempMotorController;
          Serial.println(mudancas);  // Imprime a string com as mudanças detalhadas
        }
      }

      if (frame.id == BASE_BATTERY_ID || frame.id == BASE_CONTROLLER_ID ) {
        if (xQueueSend(canFrameQueue, &frame, 0) != pdTRUE) {
          logMessage("⚠️ Fila cheia! Frame real descartado");
        }
      }
    }
    vTaskDelay(50);
  }
}

void httpSenderTask(void* pv) {
  // Conectar Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    logMessage("📶 Conectando ao Wi-Fi...");
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
  logMessage("✅ Wi-Fi conectado. IP: %s", WiFi.localIP().toString().c_str());

  // Cadastrar dispositivo
  while (!dispositivoRegistrado) {
    logMessage("📝 Tentando cadastrar dispositivo...");
    if (!cadastrarDispositivo()) {
      vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
  }

  // Loop de envio
  unsigned long lastSend = millis();
  while (1) {
    int count = uxQueueMessagesWaiting(canFrameQueue);
    unsigned long now = millis();

    if (count >= HTTP_SEND_THRESHOLD || (now - lastSend >= HTTP_SEND_INTERVAL_MS && count > 0)) {
      CanMessage batch[HTTP_SEND_THRESHOLD];
      int n = 0;
      while (n < HTTP_SEND_THRESHOLD && xQueueReceive(canFrameQueue, &batch[n], 0)) n++;

      if (n == 0) {
        vTaskDelay(10);
        continue;
      }

      // Reconnect if needed
      if (WiFi.status() != WL_CONNECTED) {
        WiFi.reconnect();
        vTaskDelay(2000);
        if (WiFi.status() != WL_CONNECTED) continue;
      }

      // ✅ Montar JSON como ARRAY DIRETO (sem envelope)
      DynamicJsonDocument doc(5120);
      JsonArray frames = doc.to<JsonArray>();  // <-- doc é convertido em array

      for (int i = 0; i < n; i++) {
        JsonObject frame = frames.createNestedObject();
        frame["canId"] = batch[i].id;
        frame["dlc"] = batch[i].length;
        frame["rtr"] = batch[i].isExtended;

        // ✅ "data" como array de números (ex: [27, 143, 97, ...])
        JsonArray data = frame.createNestedArray("data");
        for (int j = 0; j < batch[i].length; j++) {
          data.add(batch[i].data[j]);  // byte como número
        }
      }


      String json;
      serializeJson(doc, json);
      // Concatena a base da URL com o deviceId
      String fullUrl = String(TELEMETRY_URL) + deviceId;
      // Enviar
      HTTPClient http;
      http.setTimeout(10000);
      if (http.begin(fullUrl)) {
        http.addHeader("Content-Type", "application/json");
        int code = http.POST(json);
        if (code == 201) {
          logMessage("📤 Enviado lote de %d frames. HTTP: %d", n, code);
        } else {
          logMessage("❌ 📤 Erro em lote de %d frames. HTTP: %d", n, code);
        }
        http.end();
      } else {
        logMessage("❌ Falha ao iniciar HTTP");
      }

      lastSend = now;
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

// -----------------------------
// Setup
// -----------------------------
void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  // Filas
  logMessageQueue = xQueueCreate(20, MAX_LOG_MESSAGE_LEN);
  canFrameQueue = xQueueCreate(CAN_QUEUE_SIZE, sizeof(CanMessage));
  if (!logMessageQueue || !canFrameQueue) {
    Serial.println("ERRO: Falha ao criar filas!");
    while (1) delay(100);
  }

  // Iniciar CAN (só em modo real)
  if (!TESTMODE) {
    ESP32Can.setPins(CAN_TX_PIN, CAN_RX_PIN);
    if (!ESP32Can.begin(CAN_SPEED)) {
      logMessage("❌ Falha ao iniciar CAN!");
      while (1) delay(100);
    }
    logMessage("✅ CAN iniciado (250 kbps)");
  }

  // Criar tasks
  if (TESTMODE) {
    logMessage("[INFO] Modo SIMULAÇÃO ativo");
    xTaskCreate(canSimTask, "CAN Sim", 4096, NULL, 2, NULL);
  } else {
    logMessage("[INFO] Modo CAN REAL ativo");
    xTaskCreate(canTask, "CAN Reader", 4096, NULL, 2, NULL);
  }

  if (DEBUGMODE) {
    xTaskCreate(debugTask, "Debug Task", 2048, NULL, 0, NULL);
  }

  xTaskCreate(httpSenderTask, "HTTP Sender", 10000, NULL, 1, NULL);
  xTaskCreate(serialLoggerTask, "Logger", 2048, NULL, 0, NULL);

  logMessage("🟢 Sistema iniciado. Aguardando cadastro...");
}

void loop() {
  vTaskDelay(1000 / portTICK_PERIOD_MS);
}
