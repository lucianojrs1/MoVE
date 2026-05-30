#include <WiFi.h>
#include <WebSocketsClient.h> // Biblioteca WebSocket

const char* ssid = "Salvacao_2_conto";
const char* password = "mimda2conto";
const char* serverAddress = "192.168.1.160"; // Ex: "192.168.0.100"
const uint16_t serverPort = 3001; // Mesma porta do servidor Node.js

WebSocketsClient webSocket;

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
    switch(type) {
        case WStype_DISCONNECTED:
            Serial.println("[WSc] Disconnected!");
            break;
        case WStype_CONNECTED:
            Serial.print("[WSc] Connected to url: ");
            Serial.println((char*)payload);
            // Envia uma mensagem ao servidor após conectar
            webSocket.sendTXT("ESP32 conectado ao WebSocket!");
            break;
        case WStype_TEXT:
            Serial.print("[WSc] Received: ");
            Serial.println((char*)payload);
            // Aqui você pode processar a mensagem recebida do servidor
            break;
    }
}

void setup() {
    Serial.begin(115200);

    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Connecting to WiFi...");
    }
    Serial.println("WiFi connected!");

    // Configura o WebSocket
    webSocket.begin(serverAddress, serverPort, "/");
    webSocket.onEvent(webSocketEvent); // Registra callback
}

void loop() {
    webSocket.loop(); // Processa eventos WebSocket

    // Exemplo: envia dados CAN a cada 2 segundos
    static unsigned long lastSend = 0;
    if (millis() - lastSend > 2000) {
        String data = "CAN_DATA_EXAMPLE"; // Substitua por dados reais
        webSocket.sendTXT(data);
        lastSend = millis();
    }
}