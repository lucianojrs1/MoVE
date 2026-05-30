#include <Arduino.h>
#include <driver/twai.h>
#include "../../config/constants.h"


// Configuração dos pinos CAN
#define CAN_TX_PIN GPIO_NUM_5
#define CAN_RX_PIN GPIO_NUM_4

//======= CONFIGURAÇÕES CAN do TCC =======
#define N_BATTERIES 1              // Number of batteries in the system

// Creates a struct to store bateries medium frequency info
struct batteryInfo {
  int current = 0;
  int voltage = 0;
  int SoC = 0;
  int SoH = 0;
  int temperature = 0;
  int capacity = 0;
};
// Instantiate an array with all baterries configured
struct batteryInfo batteries[N_BATTERIES];
// Creates a struct to store powertrain's data
struct powertrainInfo {
  int motorSpeedRPM = 0;
  int motorTorque = 0;
  int motorTemperature = 0;
  int controllerTemperature = 0;
};
// Instantiate the powertrain structure
struct powertrainInfo CurrentPowertrainData;
// Creates a struct to store MCU's error log
struct ControllerErrorInfo {
  int hardwareFault1 = 0;          // hardware fault
  int motorSensor = 0;             // motor sensor error
  int overVoltage = 0;             // over voltage
  int underVoltage = 0;            // under voltage
  int overTemperature = 0;         // over temperatue
  int overCurrent = 0;             // over current
  int overLoad = 0;                // over load
  int motorLock = 0;               // motor lock protection
  int hardwareFault2 = 0;          // hardware fault
  int hardwareFault3 = 0;          // hardware fault
  int motorSensorNotConnected = 0; // motor sensor not connect
  int hardwareFault4 = 0;          // hardware fault
  int hardwareFault5 = 0;          // hardware fault
  int motorTempSensShort = 0;      // motor temperature sensor short
  int motorTempSensOpen = 0;       // motor temperature sensor open
};
//  Instantiate the err structure
struct ControllerErrorInfo mcuError;
// Creates a struct to store BMS error data
struct BMSErrorInfo {
  int W_cell_chg = 0;               // cell over charge warning
  int E_cell_chg = 0;               // cell over charge error
  int W_pkg_overheat = 0;           // pack charge over heat warning
  int E_pkg_chg_overheat = 0;       // pack charge over heat error
  int W_pkg_chg_undertemp = 0;      // pack charge low temperatue warning
  int E_pkg_chg_undertemp = 0;      // pack charge low temperatue error
  int W_pkg_chg_overcurrent = 0;    // pack chare over current warning
  int E_pkg_chg_overcurrent = 0;    // pack chare over current error
  int W_pkg_overvoltage = 0;        // pack over voltage warning
  int E_pkg_overvoltage = 0;        // pack over voltage error
  int E_charger_COM = 0;            // communication error with charger
  int E_pkg_chg_softstart = 0;      // pack charge soft start error
  int E_chg_relay_stuck = 0;        // charging relay stuck
  int W_cell_dchg_undervoltage = 0; // cell discharge under voltage warning
  int E_cell_dchg_undervoltage = 0; // cell discharge under voltage error
  int E_cell_deep_undervoltage = 0; // cell deep under voltage
  int W_pkg_dchg_overheat = 0;      // pack discharge over heat warning
  int E_pkg_dchg_overheat = 0;      // pack discharge over heat error
  int W_pkg_dchg_undertemp = 0;     // discharge low temperature warning
  int E_pkg_dchg_undertemp = 0;     // pack discharge low temperature error
  int W_pkg_dchg_overcurrent = 0;   // pack dischage over current waning
  int E_pkg_dchg_overcurrent = 0;   // pack dischage over current error
  int W_pkg_undervoltage = 0;       // pack under voltage warning
  int E_pkg_undervoltage = 0;       // pack under voltage  error
  int E_VCU_COM = 0;                // Communication error to VCU
  int E_pkg_dchg_softstart = 0;     // pack discharge soft start error
  int E_dchg_relay_stuck = 0;       // discharging relay stuck
  int E_pkg_dchg_short = 0;         // pack discharge short
  int E_pkg_temp_diff = 0;          // pack excessive temperature differentials
  int E_cell_voltage_diff = 0;      // cell excessive voltage differentials
  int E_AFE = 0;                    // AFE Error
  int E_MOS_overtemp = 0;           // MOS over temperature
  int E_external_EEPROM = 0;        // external EEPROM failure
  int E_RTC = 0;                    // RTC failure
  int E_ID_conflict = 0;            // ID conflict
  int E_CAN_msg_miss = 0;           // CAN message miss
  int E_pkg_voltage_diff = 0;       // pack excessive voltage differentials
  int E_chg_dchg_current_conflict = 0; // charge and discharge current conflict
  int E_cable_abnormal = 0;            // cable abnormal
};
//  Instantiate the err structure
struct BMSErrorInfo bmsError[N_BATTERIES];
// ========== END CONFIGURAÇÕES CAN ==========

// TaskHandle
TaskHandle_t xHandleCANReader = NULL;

// Função da tarefa para ler mensagens CAN
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

void setup() {
  Serial.begin(115200);
  delay(500);

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

  // Cria a tarefa FreeRTOS
  xTaskCreate(taskCANReader, "CAN_Reader", 4096, NULL, 2, &xHandleCANReader);
}

void loop() {
  vTaskDelay(1000); // Mantém o sistema rodando
}

// Não usada
void DecodeCANMessage(twai_message_t *message) {

  // Loop control variables
  bool already_checked[6] = {false, false, false,
                             false, false, false}; //  IDs already checked
  int16_t packetId = 0x000;
  int16_t packetSize = 10;
  // int16_t packetId = message.identifier;
  // int16_t packetSize = message.data_length_code;
  int16_t packetData[packetSize];

  if ((packetId == BASE_BATTERY_ID && !already_checked[0]) ||
      (packetId == BASE_BATTERY_ID + 1 && !already_checked[1])) {

    int16_t index = 0;
    if (packetId == BASE_BATTERY_ID) {
      index = 0;
    } else {
      index = 1;
    }

    // Read the BMS1 Data and save it to the gloabl state
    // clang-format off
    batteries[index].current        = (packetData[2] * (int)pow(16, 2) + packetData[3]) * 0.1;
    batteries[index].voltage        = (packetData[0] * (int)pow(16, 2) + packetData[1]) * 0.1; // deslocamento para a esqueda
    batteries[index].SoC            = packetData[6];
    batteries[index].SoH            = packetData[7];
    batteries[index].temperature    = packetData[4];
    // clang-format on

    // Flag this packet as already checked
    already_checked[index] = true;

  } else if (packetId == BASE_CONTROLLER_ID && !already_checked[2]) {

    // Read the controller data and save it to the gloabl state
    // clang-format off
    CurrentPowertrainData.motorSpeedRPM         = packetData[0] * (int)pow(16, 2) + packetData[1];
    CurrentPowertrainData.motorTorque           = (packetData[2] * (int)pow(16, 2) + packetData[3]) * 0.1;
    CurrentPowertrainData.motorTemperature      = packetData[7] - 40;
    CurrentPowertrainData.controllerTemperature = packetData[6] - 40;
    // clang-format on
    // Flag this packet as already checked
    already_checked[2] = true;

  } else if (packetId == BASE_CONTROLLER_ID_2 && !already_checked[3]) {

    // clang-format off
    mcuError.hardwareFault1          = ( packetData[2] & (0x80 >> 0) ) >> (7 - 0);
    mcuError.motorSensor             = ( packetData[2] & (0x80 >> 1) ) >> (7 - 1);
    mcuError.overVoltage             = ( packetData[2] & (0x80 >> 2) ) >> (7 - 2);
    mcuError.underVoltage            = ( packetData[2] & (0x80 >> 3) ) >> (7 - 3);
    mcuError.overTemperature         = ( packetData[2] & (0x80 >> 4) ) >> (7 - 4);
    mcuError.overCurrent             = ( packetData[2] & (0x80 >> 5) ) >> (7 - 5);
    mcuError.overLoad                = ( packetData[2] & (0x80 >> 6) ) >> (7 - 6);
    mcuError.motorLock               = ( packetData[2] & (0x80 >> 7) ) >> (7 - 7);
    mcuError.hardwareFault2          = ( packetData[3] & (0x80 >> 0) ) >> (7 - 0);
    mcuError.hardwareFault3          = ( packetData[3] & (0x80 >> 1) ) >> (7 - 1);
    mcuError.motorSensorNotConnected = ( packetData[3] & (0x80 >> 2) ) >> (7 - 2);
    mcuError.hardwareFault4          = ( packetData[3] & (0x80 >> 3) ) >> (7 - 3);
    mcuError.hardwareFault5          = ( packetData[3] & (0x80 >> 4) ) >> (7 - 4);
    mcuError.motorTempSensShort      = ( packetData[3] & (0x80 >> 5) ) >> (7 - 5);
    mcuError.motorTempSensOpen       = ( packetData[3] & (0x80 >> 6) ) >> (7 - 6);
    // clang-format on

  } else if ((packetId == BASE_BATTERY_ID_2 && !already_checked[4]) ||
             (packetId == BASE_BATTERY_ID_2 + 1 && !already_checked[5])) {

    int16_t index = 0;
    int16_t idOffset = 4;
    if (packetId == BASE_BATTERY_ID_2) {
      index = 0;
    } else {
      index = 1;
    }

    // clang-format off
    bmsError[index].W_cell_chg                  = ( packetData[0] & (0x80 >> 0) ) >> (7 - 0);
    bmsError[index].E_cell_chg                  = ( packetData[0] & (0x80 >> 1) ) >> (7 - 1);
    bmsError[index].W_pkg_overheat              = ( packetData[0] & (0x80 >> 2) ) >> (7 - 2);
    bmsError[index].E_pkg_chg_overheat          = ( packetData[0] & (0x80 >> 3) ) >> (7 - 3);
    bmsError[index].W_pkg_chg_undertemp         = ( packetData[0] & (0x80 >> 4) ) >> (7 - 4);
    bmsError[index].E_pkg_chg_undertemp         = ( packetData[0] & (0x80 >> 5) ) >> (7 - 5);
    bmsError[index].W_pkg_chg_overcurrent       = ( packetData[0] & (0x80 >> 6) ) >> (7 - 6);
    bmsError[index].E_pkg_chg_overcurrent       = ( packetData[0] & (0x80 >> 7) ) >> (7 - 7);
    bmsError[index].W_pkg_overvoltage           = ( packetData[1] & (0x80 >> 0) ) >> (7 - 0);
    bmsError[index].E_pkg_overvoltage           = ( packetData[1] & (0x80 >> 1) ) >> (7 - 1);
    bmsError[index].E_charger_COM               = ( packetData[1] & (0x80 >> 2) ) >> (7 - 2);
    bmsError[index].E_pkg_chg_softstart         = ( packetData[1] & (0x80 >> 3) ) >> (7 - 3);
    bmsError[index].E_chg_relay_stuck           = ( packetData[1] & (0x80 >> 4) ) >> (7 - 4);
    bmsError[index].W_cell_dchg_undervoltage    = ( packetData[2] & (0x80 >> 0) ) >> (7 - 0);
    bmsError[index].E_cell_dchg_undervoltage    = ( packetData[2] & (0x80 >> 1) ) >> (7 - 1);
    bmsError[index].E_cell_deep_undervoltage    = ( packetData[2] & (0x80 >> 2) ) >> (7 - 2);
    bmsError[index].W_pkg_dchg_overheat         = ( packetData[2] & (0x80 >> 3) ) >> (7 - 3);
    bmsError[index].E_pkg_dchg_overheat         = ( packetData[2] & (0x80 >> 4) ) >> (7 - 4);
    bmsError[index].W_pkg_dchg_undertemp        = ( packetData[2] & (0x80 >> 5) ) >> (7 - 5);
    bmsError[index].E_pkg_dchg_undertemp        = ( packetData[2] & (0x80 >> 6) ) >> (7 - 6);
    bmsError[index].W_pkg_dchg_overcurrent      = ( packetData[2] & (0x80 >> 7) ) >> (7 - 7);
    bmsError[index].E_pkg_dchg_overcurrent      = ( packetData[3] & (0x80 >> 0) ) >> (7 - 0);
    bmsError[index].W_pkg_undervoltage          = ( packetData[3] & (0x80 >> 1) ) >> (7 - 1);
    bmsError[index].E_pkg_undervoltage          = ( packetData[3] & (0x80 >> 2) ) >> (7 - 2);
    bmsError[index].E_VCU_COM                   = ( packetData[3] & (0x80 >> 3) ) >> (7 - 3);
    bmsError[index].E_pkg_dchg_softstart        = ( packetData[3] & (0x80 >> 4) ) >> (7 - 4);
    bmsError[index].E_dchg_relay_stuck          = ( packetData[3] & (0x80 >> 5) ) >> (7 - 5);
    bmsError[index].E_pkg_dchg_short            = ( packetData[3] & (0x80 >> 6) ) >> (7 - 6);
    bmsError[index].E_pkg_temp_diff             = ( packetData[6] & (0x80 >> 0) ) >> (7 - 0);
    bmsError[index].E_cell_voltage_diff         = ( packetData[6] & (0x80 >> 1) ) >> (7 - 1);
    bmsError[index].E_AFE                       = ( packetData[6] & (0x80 >> 2) ) >> (7 - 2);
    bmsError[index].E_MOS_overtemp              = ( packetData[6] & (0x80 >> 3) ) >> (7 - 3);
    bmsError[index].E_external_EEPROM           = ( packetData[6] & (0x80 >> 4) ) >> (7 - 4);
    bmsError[index].E_RTC                       = ( packetData[6] & (0x80 >> 5) ) >> (7 - 5);
    bmsError[index].E_ID_conflict               = ( packetData[6] & (0x80 >> 6) ) >> (7 - 6);
    bmsError[index].E_CAN_msg_miss              = ( packetData[6] & (0x80 >> 7) ) >> (7 - 7);
    bmsError[index].E_pkg_voltage_diff          = ( packetData[7] & (0x80 >> 0) ) >> (7 - 0);
    bmsError[index].E_chg_dchg_current_conflict = ( packetData[7] & (0x80 >> 1) ) >> (7 - 1);
    bmsError[index].E_cable_abnormal            = ( packetData[7] & (0x80 >> 2) ) >> (7 - 2);
    // clang-format on

    // Flag this packet as already checked
    already_checked[index + idOffset] = true;
  }
}
