/**
 * ============================================
 * 🎯 dom-elements.js - Seletores do DOM
 * ============================================
 * Centraliza todos os seletores de elementos HTML
 * para facilitar manutenção e evitar repetição.
 * 
 * @module domElements
 */

// 🔹 Elementos de Dados BMS
export const bmsElements = {
  current: document.getElementById('bms-current'),
  voltage: document.getElementById('bms-voltage'),
  soc: document.getElementById('bms-soc'),
  soh: document.getElementById('bms-soh'),
  temp: document.getElementById('bms-temp')
};

// 🔹 Elementos de Dados do Controller/Motor
export const controllerElements = {
  modo: document.getElementById('modo'),
  rpm: document.getElementById('rpm'),
  torque: document.getElementById('torque'),
  tempMotor: document.getElementById('temp-motor'),
  tempBatt: document.getElementById('temp-batt')
};

// 🔹 Elementos de Localização e GPS
export const gpsElements = {
  location: document.getElementById('location'),
  btn: document.getElementById('gps-btn'),
  label: document.getElementById('gps-label'),
  status: document.getElementById('gps-status'),
  startBtn: document.getElementById('start-gps-btn')
};

// 🔹 Elementos de UI Geral
export const uiElements = {
  themeToggle: document.getElementById('theme-toggle'),
  statusServer: document.getElementById('status-indicator-server'),
  statusEsp: document.getElementById('status-indicator-esp'),
  downloadCan: document.getElementById('download-can-data'),
  downloadDevice: document.getElementById('download-device-data')
};

// 🔹 Elementos de Tabelas
export const tableElements = {
  canBody: document.querySelector('#can-body'),
  vehicleBody: document.getElementById('vehicle-data-body')
};

// 🔹 Elemento do Mapa
export const mapElement = document.getElementById('map');

/**
 * Verifica se todos os elementos críticos existem
 * @returns {boolean} True se todos os elementos necessários foram encontrados
 */
export function validateElements() {
  const critical = [
    ...Object.values(bmsElements),
    ...Object.values(controllerElements),
    ...Object.values(tableElements),
    mapElement
  ].filter(el => el !== null && el !== undefined);
  
  if (critical.length === 0) {
    console.warn('⚠️ Alguns elementos do DOM não foram encontrados. Verifique o HTML.');
    return false;
  }
  return true;
}

export default {
  bmsElements,
  controllerElements,
  gpsElements,
  uiElements,
  tableElements,
  mapElement,
  validateElements
};
