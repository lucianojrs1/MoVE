#include <ESP32-TWAI-CAN.hpp> // Biblioteca ESP32-TWAI-CAN

// ------------------------------------------------------------------
// --- CONFIGURAÇÃO DE PINOS E VELOCIDADE ---
// ------------------------------------------------------------------
#define CAN_TX_PIN 5      // GPIO 5 (TXD)
#define CAN_RX_PIN 4      // GPIO 4 (RXD)

// CORRIGIDO 1: A biblioteca exige o tipo 'TwaiSpeed' e suas constantes internas.
// Usamos TwaiSpeed e a constante para 250kbps.
const TwaiSpeed CAN_SPEED = TWAI_SPEED_250KBPS;

// Estrutura nativa para frames (a única que funciona com readFrame)
twai_message_t rxFrame; 

// ------------------------------------------------------------------
// --- FUNÇÃO DE CONFIGURAÇÃO (SETUP) ---
// ------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10); 
  
  Serial.println("--- Leitor/Sniffer CAN ESP32 (TJA1050) - Versão Final ---");
  
  // 1. Configura os pinos antes de iniciar o driver CAN
  ESP32Can.setPins(CAN_TX_PIN, CAN_RX_PIN);

  // 2. Inicializa o controlador CAN. Agora passa a constante TwaiSpeed.
  if (ESP32Can.begin(CAN_SPEED)) {
    Serial.println("Controlador CAN (TWAI) iniciado com sucesso!");
    Serial.println("Monitorando em 250 kbps nos pinos TX:5 e RX:4...");
  } else {
    Serial.println("ERRO: Falha ao iniciar o controlador CAN! Verifique as conexões.");
    while (1) delay(100); 
  }
}

// ------------------------------------------------------------------
// --- LOOP PRINCIPAL (LEITURA DE DADOS) ---
// ------------------------------------------------------------------
void loop() {
  // Tenta ler um pacote (frame) do barramento.
  if (ESP32Can.readFrame(&rxFrame)) {
    
    Serial.println("---------------------------------------------");
    Serial.println("PACOTE RECEBIDO:");

    // 1. ID da Mensagem
    Serial.print("  ID: 0x");
    
    // CORRIGIDO 2: Acessa o bit EXT_FLAG (0x01) na union flags para verificar ID Estendido
    // Esta é a forma correta para o driver TWAI.
    if (rxFrame.flags & TWAI_MSG_FLAG_EXTD) { 
      Serial.print(rxFrame.identifier, HEX);
      Serial.print(" (Estendido)");
    } else {
      Serial.print(rxFrame.identifier, HEX);
      Serial.print(" (Padrão)");
    }
    
    // 2. Comprimento dos Dados (DLC)
    Serial.print(", DLC: ");
    Serial.println(rxFrame.data_length_code); // Este membro já estava correto

    // 3. Dados (Payload)
    Serial.print("  Dados (HEX): ");
    
    // Usa 'data_length_code' para limitar o loop de dados.
    for (int i = 0; i < rxFrame.data_length_code; i++) {
      byte dataByte = rxFrame.data[i];
      
      // Formata a saída para 2 dígitos em HEX
      if (dataByte < 0x10) Serial.print("0"); 
      Serial.print(dataByte, HEX);
      Serial.print(" ");
    }
    
    Serial.println();
  }
  
  delay(1); 
}
