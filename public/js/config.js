/**
 * ============================================
 * 📦 config.js - Configurações Globais
 * ============================================
 * Módulo responsável por centralizar constantes
 * e configurações usadas em todo o dashboard.
 * 
 * @module config
 */

// 🔹 Endpoints da API REST
export const API = {
  DEVICE_DATA: '/api/device',           // Dados decodificados do veículo
  CAN_DATA: '/api/can-data',            // Frames CAN brutos (últimos 50)
  EXPORT_CAN: '/api/export-can-data-csv',    // Exportar CAN para CSV
  EXPORT_VEHICLE: '/api/export-vehicle-data-csv', // Exportar veículo para CSV
  LOCATION: '/api/device/location'      // Endpoint para enviar GPS
};

// 🔹 Configurações do WebSocket
export const WS = {
  // Constrói URL dinâmica baseada no host atual
  getUrl: () => `ws://${window.location.hostname}:${window.location.port}`,
  RECONNECT_DELAY: 3000  // Delay para reconexão em ms
};

// 🔹 Configurações do GPS
export const GPS = {
  DEVICE_ID: 'esp32-moto-001',  // Identificador único do dispositivo
  WATCH_OPTIONS: {
    enableHighAccuracy: true,  // Prioriza GPS em vez de Wi-Fi/celular
    timeout: 10000,            // Timeout de 10s para obter posição
    maximumAge: 30000          // Aceita posição cacheada de até 30s
  },
  UPDATE_INTERVAL: 2000  // Intervalo de atualização da UI (ms)
};

// 🔹 Configurações do Mapa (Leaflet)
export const MAP = {
  INITIAL_COORDS: [-8.055581, -34.951640],  // [lat, lon] - Recife/PE
  INITIAL_ZOOM: 15,
  TILE_LAYER: 'https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png',
  TILE_ATTRIBUTION: '&copy; <a href="https://www.openstreetmap.org/copyright">OpenStreetMap</a> contributors',
  MARKER_ICON: {
    url: 'https://www.jav.com.br/wp-content/uploads/2017/03/map-marker-icon.png',
    size: [32, 32],
    anchor: [16, 32],
    popupAnchor: [0, -32]
  }
};

// 🔹 Configurações da UI
export const UI = {
  MAX_CAN_ROWS: 20,           // Máximo de linhas na tabela CAN
  HIGHLIGHT_DURATION: 500,    // Duração do efeito highlight (ms)
  FETCH_INTERVAL: 2000,       // Intervalo para polling da API (ms)
  THEME_KEY: 'theme'          // Chave para localStorage do tema
};

// Exporta configuração combinada para conveniência
export default { API, WS, GPS, MAP, UI };
