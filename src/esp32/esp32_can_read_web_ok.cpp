#include <WiFi.h>
#include <WebServer.h>
#include <ESP32-TWAI-CAN.hpp> 
#include <FS.h>           // Necessário para o sistema de arquivos
#include <LittleFS.h>     // Necessário para o LittleFS
#include "../../config/constants.h"

// ------------------------------------------------------------------
// 1. CONFIGURAÇÕES DE REDE
// ------------------------------------------------------------------
const char* ssid = "CINGUESTS";         // Credenciais preenchidas
const char* password = "acessocin";       // Credenciais preenchidas

// ------------------------------------------------------------------
// 2. CONFIGURAÇÕES GERAIS
// ------------------------------------------------------------------
#define CAN_TX_PIN 5
#define CAN_RX_PIN 4
const TwaiSpeed CAN_SPEED = TWAI_SPEED_250KBPS;
const char* LOG_FILE_NAME = "/can_log.csv"; // Arquivo salvo na memória Flash
twai_message_t rxFrame; 

WebServer server(80);

// ------------------------------------------------------------------
// 3. FUNÇÕES DE SUPORTE
// ------------------------------------------------------------------

/**
 * @brief Conecta o ESP32 à rede Wi-Fi e mostra apenas o IP no terminal.
 */
void setupWiFi() {
  Serial.print("Conectando a ");
  Serial.print(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  // Apenas a linha do IP no terminal
  Serial.println(); 
  Serial.println(WiFi.localIP()); 
}

/**
 * @brief Formata o frame CAN em uma linha CSV e salva na Flash.
 */
void logCanFrame(const twai_message_t& rx) {
    File file = LittleFS.open(LOG_FILE_NAME, FILE_APPEND);
    if (!file) {
        Serial.println("ERRO: Falha ao abrir arquivo para escrita.");
        return;
    }

    String logLine = "";
    logLine += String(millis()); logLine += ",";
    
    bool isExtended = (rx.flags & TWAI_MSG_FLAG_EXTD);
    char idHex[10];
    sprintf(idHex, "0x%lX", rx.identifier);
    logLine += idHex; logLine += ",";
    logLine += isExtended ? "E" : "S"; logLine += ",";
    logLine += String(rx.data_length_code); logLine += ",";

    for (int i = 0; i < rx.data_length_code; i++) {
        char byteHex[3];
        sprintf(byteHex, "%02X", rx.data[i]); 
        logLine += byteHex;
    }
    logLine += "\n";

    file.print(logLine);
    file.close();
    
    // Serial.print("Log salvo: ");
    // Serial.print(logLine); // Comentado para otimizar o desempenho do log
}

// ------------------------------------------------------------------
// 4. HANDLERS DO SERVIDOR WEB
// ------------------------------------------------------------------

/**
 * @brief Envia a página HTML principal.
 */
void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
  <meta charset="UTF-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>ESP32 CAN Datalogger (Flash)</title>
  <style>
    * {
      box-sizing: border-box;
      margin: 0;
      padding: 0;
    }

    body {
      font-family: system-ui, -apple-system, 'Segoe UI', Roboto, Arial, sans-serif;
      background: linear-gradient(135deg, #f5f7fa 0%, #e4edf5 100%);
      color: #333;
      line-height: 1.6;
      padding: 20px;
      min-height: 100vh;
    }

    .container {
      max-width: 600px;
      margin: 40px auto;
      background: white;
      padding: 32px;
      border-radius: 12px;
      box-shadow: 0 6px 20px rgba(0, 0, 0, 0.08);
      text-align: center;
    }

    h1 {
      font-size: 1.8rem;
      margin-bottom: 16px;
      color: #2c3e50;
    }

    p {
      color: #555;
      margin-bottom: 24px;
      font-size: 1.05rem;
    }

    .btn {
      display: inline-flex;
      align-items: center;
      justify-content: center;
      padding: 12px 24px;
      margin: 8px;
      font-size: 1rem;
      font-weight: 600;
      border: none;
      border-radius: 8px;
      cursor: pointer;
      transition: all 0.2s ease;
      text-decoration: none;
      color: white;
      min-width: 200px;
    }

    #download-btn {
      background: #2196F3; /* Azul moderno */
    }

    #delete-btn {
      background: #f44336; /* Vermelho suave */
    }

    .btn:hover {
      transform: translateY(-2px);
      box-shadow: 0 4px 10px rgba(0, 0, 0, 0.15);
      opacity: 1;
    }

    .btn:active {
      transform: translateY(0);
    }

    .log-count, .status {
      margin-top: 20px;
      padding: 12px;
      background-color: #f9fbfd;
      border-radius: 8px;
      font-size: 1.05rem;
      color: #2c3e50;
    }

    .log-count b, .status b {
      color: #1976D2;
    }

    .warning {
      margin-top: 24px;
      padding: 12px;
      background-color: #fff8e1;
      border-left: 4px solid #ffc107;
      border-radius: 6px;
      font-size: 0.9rem;
      color: #5d4037;
      text-align: left;
    }

    .warning::before {
      content: "⚠️ ";
      font-size: 1.2em;
    }

    @media (max-width: 600px) {
      .container {
        padding: 24px 16px;
      }

      .btn {
        width: 100%;
        margin: 8px 0;
      }
    }
  </style>
</head>
<body>

<div class="container">
  <h1>ESP32 Datalogger CAN</h1>
  <p>📝 Status do Log: Salvo na Memória Flash Interna.</p>

  <a href="/download" class="btn" id="download-btn">
    📥 BAIXAR ARQUIVO DE LOG (can_log.csv)
  </a>

  <button class="btn" id="delete-btn" onclick="confirmDelete()">
    🗑️ APAGAR LOG E LIBERAR ESPAÇO
  </button>

  <div class="log-count" id="log-count-info">
    Carregando informações...
  </div>

  <div class="status" id="free-space">
    Verificando espaço livre...
  </div>

  <div class="warning">
    <small>A gravação constante na memória flash tem vida útil limitada. Use com moderação.</small>
  </div>
</div>

<script>
function getInfo() {
    // Espaço livre
    var xhttpFree = new XMLHttpRequest();
    xhttpFree.onreadystatechange = function() {
        if (this.readyState == 4 && this.status == 200) {
            document.getElementById("free-space").innerHTML = "💾 Espaço Livre Restante: <b>" + this.responseText + "</b>";
        }
    };
    xhttpFree.open("GET", "/freespace", true);
    xhttpFree.send();

    // Contagem de logs
    var xhttpInfo = new XMLHttpRequest();
    xhttpInfo.onreadystatechange = function() {
        if (this.readyState == 4 && this.status == 200) {
            var parts = this.responseText.split(',');
            if (parts.length === 2) {
                var infoText = "📊 Frames Registrados: <b>" + parts[0] + "</b> | 📏 Tamanho Total: <b>" + parts[1] + "</b>";
                document.getElementById("log-count-info").innerHTML = infoText;
            }
        }
    };
    xhttpInfo.open("GET", "/loginfo", true);
    xhttpInfo.send();
}

function confirmDelete() {
    if (confirm("⚠️ Tem certeza que deseja apagar PERMANENTEMENTE o arquivo de log?")) {
        window.location.href = '/delete';
    }
}

window.onload = getInfo;
setInterval(getInfo, 5000);
</script>

</body>
</html>
)rawliteral";
  server.send(200, "text/html", html);
}

/**
 * @brief Implementação de handleDownload e handleDelete (sem mudanças)
 */
void handleDownload() {
  File downloadFile = LittleFS.open(LOG_FILE_NAME, FILE_READ);
  if (!downloadFile) {
    server.send(404, "text/plain", "Arquivo de log não encontrado ou vazio.");
    return;
  }
  server.sendHeader("Content-Type", "text/csv");
  server.sendHeader("Content-Disposition", "attachment; filename=" + String(LOG_FILE_NAME));
  server.sendHeader("Connection", "close");
  server.streamFile(downloadFile, "application/octet-stream");
  downloadFile.close();
}

void handleDelete() {
    if (LittleFS.remove(LOG_FILE_NAME)) {
        server.send(200, "text/plain", "Arquivo de log apagado com sucesso! Redirecionando...");
    } else {
        server.send(500, "text/plain", "Falha ao apagar o arquivo de log.");
    }
    server.sendHeader("Refresh", "3; url=/"); 
}

/**
 * @brief Retorna o ESPAÇO LIVRE em formato formatado (KB/MB).
 */
void handleFreeSpace() {
    uint32_t totalBytes = LittleFS.totalBytes();
    uint32_t usedBytes = LittleFS.usedBytes();
    uint32_t freeBytes = totalBytes - usedBytes;

    String freeSpaceString = "";
    
    if (freeBytes > 1024 * 1024) { 
        freeSpaceString += String((float)freeBytes / (1024.0 * 1024.0), 2); 
        freeSpaceString += " MB";
    } else if (freeBytes > 1024) { 
        freeSpaceString += String((float)freeBytes / 1024.0, 1);
        freeSpaceString += " KB";
    } else {
        freeSpaceString += String(freeBytes);
        freeSpaceString += " bytes";
    }
    
    server.send(200, "text/plain", freeSpaceString);
}


/**
 * @brief NOVO HANDLER: Retorna a contagem de linhas e o tamanho do arquivo log.
 */
void handleLogInfo() {
    File file = LittleFS.open(LOG_FILE_NAME, FILE_READ);
    if (!file) {
        server.send(200, "text/plain", "0,0 bytes");
        return;
    }

    // 1. Obter o tamanho do arquivo
    size_t fileSize = file.size();
    
    // 2. Contar linhas (logs)
    int lineCount = 0;
    while (file.available()) {
        if (file.read() == '\n') {
            lineCount++;
        }
    }
    file.close();

    // 3. Formatar o tamanho do arquivo
    String fileSizeString = "";
    if (fileSize > 1024 * 1024) { 
        fileSizeString += String((float)fileSize / (1024.0 * 1024.0), 2); 
        fileSizeString += " MB";
    } else if (fileSize > 1024) { 
        fileSizeString += String((float)fileSize / 1024.0, 1);
        fileSizeString += " KB";
    } else {
        fileSizeString += String(fileSize);
        fileSizeString += " bytes";
    }

    // Retorna no formato "LINHAS,TAMANHO_FORMATADO"
    String response = String(lineCount) + "," + fileSizeString;
    server.send(200, "text/plain", response);
}


// ------------------------------------------------------------------
// 5. SETUP E LOOP
// ------------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  delay(100);

  // 1. Inicialização da Memória Flash (LittleFS)
  if (!LittleFS.begin(true)) {
      Serial.println("ERRO: Falha ao montar o LittleFS! Verifique as partições.");
      while(true);
  }
  
  // 2. Conexão Wi-Fi (mostrará apenas o IP no final)
  setupWiFi();

  // 3. Inicializa o controlador CAN.
  ESP32Can.setPins(CAN_TX_PIN, CAN_RX_PIN);
  if (ESP32Can.begin(CAN_SPEED)) {
     Serial.println("CAN iniciado.");
  } else {
    Serial.println("ERRO: Falha ao iniciar o controlador CAN!");
    while (1) delay(100); 
  }

  // 4. Configuração do Servidor Web
  server.on("/", handleRoot);
  server.on("/download", handleDownload);      
  server.on("/delete", handleDelete);          
  server.on("/freespace", handleFreeSpace);    
  server.on("/loginfo", handleLogInfo);        // NOVO endpoint para informações do log
  server.begin();
}

void loop() {
  // 1. Processa requisições Web
  server.handleClient();
  
  // 2. Leitura CAN e Log na Flash
  if (ESP32Can.readFrame(&rxFrame)) {
    logCanFrame(rxFrame);
  }
}
