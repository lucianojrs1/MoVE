#include <HTTPClient.h>
#include <WiFi.h>
#include <Arduino.h>
#include <driver/twai.h>

// ============== CONFIGURAÇÕES ==============

// Configuração dos pinos CAN
#define CAN_TX_PIN GPIO_NUM_5
#define CAN_RX_PIN GPIO_NUM_4

String ssid = "CINGUESTS";
String password = "acessocin";

String url = "https://4ad79ad5ba79-10-244-7-39-31952.saci.r.killercoda.com/api";
// ============== Estrutura de dados CAN ==============
struct CanMessage {
  String id;
  int dlc;
  String data;
};
// Auxiliar para contagem de repetições
int sendCount = 0;
// Tamanho máximo do buffer de mensagens CAN
const int CAN_BUFFER_SIZE = 11;     // Fila suporta até 11 mensagens
const int CAN_BUFFER_AUX_SIZE = 10; // Fila suporta até 10 mensagens para enviar
const int SEND_THRESHOLD = 10;      // Enviar a cada 10 mensagens
const int CAN_SIMULATION_INTERVAL_MS = 1000; // 1 Hz
const int INTERVAL_1000MS = 1000;            // 1 Hz
const int INTERVAL_100MS = 100;              // 10 Hz
const int INTERVAL_10MS = 10;                // 100 Hz
const int INTERVAL_1MS = 1;                  // 1000 Hz
// ============== Variáveis compartilhadas ==============
CanMessage buffer[10];    // Buffer Para envio e leitura
CanMessage bufferAux[10]; // Buffer Auxiliar

QueueHandle_t canQueue;        // Fila para armazenar mensagens CAN
SemaphoreHandle_t canMutex;    // Protege o acesso à variável buffer
SemaphoreHandle_t bufferReady; // Sinaliza buffer Pronto para envio

int messageCount = 0; // Contador de mensagens CAN recebidas
uint32_t count = 0;   // Contador de mensagens na fila

// ============== Protótipo da Task ==============
void telemetriaTask(void *parameter);
void canReaderTask(void *parameter);

// ============== SETUP ==============
void setup() {
  Serial.begin(115200);
  delay(100);


  // Configuração do controlador CAN (TWAI)
  // clang-format off
  twai_general_config_t g_config    = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_PIN, CAN_RX_PIN, TWAI_MODE_NORMAL);
  twai_timing_config_t t_config     = TWAI_TIMING_CONFIG_250KBITS(); // 250 kbps
  twai_filter_config_t f_config     = TWAI_FILTER_CONFIG_ACCEPT_ALL(); // Aceita todos os IDs
  // clang-format on
  // Instala e inicia o driver CAN
  if (twai_driver_install(&g_config, &t_config, &f_config) != ESP_OK) {
    Serial.println("Falha ao instalar driver CAN!");
    return;
  }

  if (twai_start() != ESP_OK) {
    Serial.println("Falha ao iniciar CAN!");
    return;
  }

  Serial.println("CAN Iniciado com sucesso (250kbps)");


  // Cria uma fila (buffer) para armazenar mensagens CAN
  canQueue = xQueueCreate(CAN_BUFFER_SIZE, sizeof(CanMessage));
  if (canQueue == NULL) {
    Serial.println("❌ Falha ao criar fila CAN");
    while (1)
      delay(10);
  }
  // Inicializa mutex para proteger o acesso ao dado CAN
  canMutex = xSemaphoreCreateMutex();
  if (canMutex == NULL) {
    Serial.println("❌ Falha ao criar mutex para CAN");
    while (1)
      delay(10);
  }
  // Cria semáforo de sinalização
  bufferReady = xSemaphoreCreateBinary();
  if (bufferReady == NULL) {
    Serial.println("❌ Falha ao criar semáforo de buffer cheio");
    while (1)
      delay(10);
  }

  // Cria a task CAN no NÚCLEO 0
  xTaskCreatePinnedToCore(taskCANReader, "CAN_Reader", 4096, NULL,
                          2, // Prioridade maior que telemetria
                          NULL,
                          0 // Núcleo 0
  );
  
  // Cria a tarefa de telemetria no núcleo 1
  xTaskCreatePinnedToCore(telemetriaTask,   // Função da tarefa
                          "TelemetriaTask", // Nome amigável
                          10000, // Tamanho da pilha (grande para HTTP)
                          NULL,  // Parâmetros
                          1,     // Prioridade
                          NULL,  // Handle (não usado)
                          1      // Núcleo 1 (deixe o núcleo 0 livre)
  );
}
// ============== LOOP ==============
void loop() {
  // Pode ficar vazio ou rodar outras tarefas leves
  delay(1); // Necessário para evitar bloqueio
}

// =============== TASK 1: Leitura CAN ===================
void taskCANReader(void *pvParameters) {
  while (1) {
    twai_message_t message;
    // Tenta ler uma mensagem (com timeout de 100ms)
    if (twai_receive(&message, pdMS_TO_TICKS(100)) == ESP_OK) {
      Serial.printf("ID: 0x%03X, DLC: %d, Data: ", message.identifier,
                    message.data_length_code);
      for (int i = 0; i < message.data_length_code; i++) {
        Serial.printf("%02X ", message.data[i]);
      }
      Serial.println();
    }
    vTaskDelay(1); // Evita loop muito rápido
  }
}

// ============== TASK 2: Envio de Telemetria ==============
void telemetriaTask(void *parameter) {
  (void)parameter; // Ignora parâmetro não usado
  Serial.println("Conectando ao Wi-Fi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("✅ Conectado ao Wi-Fi!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  unsigned long lastSend = 0;
  for (;;) { // Loop infinito da tarefa
    unsigned long currentMillis = millis();
    if (xSemaphoreTake(bufferReady, INTERVAL_10MS / portTICK_PERIOD_MS) ==
        pdTRUE) {
      Serial.println("🚀 Sinal recebido! Enviando 10 mensagens...");
      // Protege a escrita com mutex
      if (xSemaphoreTake(canMutex, INTERVAL_10MS / portTICK_PERIOD_MS)) {
        for (int i = 0; i < SEND_THRESHOLD; i++) {
          bufferAux[i] = buffer[i];
        }
        xSemaphoreGive(canMutex);
      }
      // Verifica conexão Wi-Fi
      if (WiFi.status() != WL_CONNECTED) {
        Serial.println("❌ Wi-Fi desconectado. Tentando reconectar...");
        WiFi.reconnect();
        delay(2000);
        if (WiFi.status() != WL_CONNECTED) {
          continue;
        }
      }
      HTTPClient http;
      http.setTimeout(10000); // Timeout de 10 segundos
      // Tenta iniciar conexão
      if (!http.begin(url)) {
        Serial.println("❌ Falha ao iniciar HTTP. URL inválida?");
        continue;
      }

      http.addHeader("Content-Type", "application/json");

      String jsonData = "{";
      jsonData += "\"speed\": 48,";
      jsonData += "\"battery\": {\"soc\": 76, \"soh\": 94, \"voltage\": 71.8, "
                  "\"current\": -3.4, \"temperature\": 31.2},";
      jsonData += "\"motor\": {\"rpm\": 3600, \"power\": 9.8, \"regenLevel\": "
                  "40, \"motorTemp\": 68, \"inverterTemp\": 61},";
      jsonData += "\"location\": {\"type\": \"Point\", \"coordinates\": "
                  "[-45.6333, -23.5500]},";
      jsonData += "\"driveMode\": \"sport\",";
      jsonData += "\"range\": 74,";
      jsonData += "\"vehicleStatus\": \"ligado\",";
      jsonData += "\"odometer\": 1247.3,";
      jsonData += "\"alerts\": [{";
      jsonData += "  \"code\": \"MOTOR_OVERHEAT_WARNING\",";
      jsonData += "  \"message\": \"Temperatura do motor acima de 65°C\",";
      jsonData += "  \"severity\": \"warning\"";
      jsonData += "}],";
      jsonData += "\"canMessages\": [";

      for (int i = 0; i < SEND_THRESHOLD; i++) {
        jsonData += "{";
        jsonData += "\"canId\": \"" + bufferAux[i].id + "\",";
        jsonData += "\"data\": \"" + bufferAux[i].data + "\",";
        jsonData +=
            "\"dlc\": " + String(bufferAux[i].dlc); // ✅ Sem vírgula aqui
        jsonData += "}";                            // Fecha o objeto

        if (i < SEND_THRESHOLD - 1) {
          jsonData += ","; // ✅ Vírgula entre objetos do array
        }
      }
      jsonData += "]"; // Fecha o array canMessages
      jsonData += "}"; // Fecha o JSON principal
      sendCount++;
      Serial.printf("%d  📤 Enviando telemetria...\n", sendCount);
      int httpResponseCode = http.POST(jsonData);

      if (httpResponseCode > 0) {
        switch (httpResponseCode) {
        case 200:
        case 201:
        case 204:
          Serial.printf("✅ Sucesso! Código HTTP: %d\n", httpResponseCode);
          break;

        case 400:
          Serial.println(
              "❌ Requisição inválida. JSON mal formado ou campos faltando.");
          break;

        case 401:
        case 403:
          Serial.println(
              "🔐 Falha de autenticação. Verifique token ou API key.");
          break;

        case 404:
          Serial.println("🔍 Endpoint não encontrado. Verifique a URL.");
          break;

        case 413:
          Serial.println("📦 Payload muito grande. Envie em lotes menores.");
          break;

        case 429:
          Serial.println("⏳ Muitas requisições. Aumente o intervalo.");
          break;

        default:
          if (httpResponseCode >= 400 && httpResponseCode < 500) {
            Serial.printf("❌ Erro do cliente: %d\n", httpResponseCode);
          } else if (httpResponseCode >= 500) {
            Serial.printf(
                "🔧 Erro do servidor: %d. Tente novamente mais tarde.\n",
                httpResponseCode);
          } else {
            Serial.printf("⚠️  Resposta inesperada: %d\n", httpResponseCode);
          }
          break;
        }
      } else {
        Serial.printf("❌ Falha na requisição. Código: %d\n", httpResponseCode);
      }
      http.end(); // Sempre encerre a conexão
    }
    // ⏱️ Libera o núcleo por 10ms (evita travar o FreeRTOS)
    vTaskDelay(INTERVAL_10MS / portTICK_PERIOD_MS);
  }
}
