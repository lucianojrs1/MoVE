/**
 * ============================================
 * 🎨 ui.js - Atualizações de Interface
 * ============================================
 * Funções para manipular display de dados,
 * tema, indicadores e interações de UI.
 * 
 * @module ui
 * @requires ../config.js
 * @requires ../utils.js
 * @requires ./dom-elements.js
 */

import { UI } from './config.js';
import { formatValue } from './utils.js';
import { bmsElements, controllerElements, uiElements, gpsElements } from './dom-elements.js';

// Estado do tema
let _currentTheme = 'light-theme';

/**
 * Atualiza display dos dados do BMS
 * @param {Object} data - Objeto com campos: current, voltage, soc, soh, temperature
 */
export function updateBmsDisplay(data) {
  
  if (!data) return;
  
  // Atualiza cada elemento com formatação adequada
  if (bmsElements.current) {
    bmsElements.current.textContent = formatValue(data.current, 2);
  }
  if (bmsElements.voltage) {
    bmsElements.voltage.textContent = formatValue(data.voltage, 3);
  }
  if (bmsElements.soc) {
    bmsElements.soc.textContent = data.soc !== undefined ? data.soc : '--';
  }
  if (bmsElements.soh) {
    bmsElements.soh.textContent = data.soh !== undefined ? data.soh : '--';
  }
  if (bmsElements.temp) {
    bmsElements.temp.textContent = data.temperature !== undefined ? data.temperature : '--';
  }
}

/**
 * Atualiza display dos dados do Controller/Motor
 * @param {Object} data - Objeto com campos: modo, rpm, torque, tempMotor, tempBatt
 */
export function updateControllerDisplay(data) {
  if (!data) return;
  
  if (controllerElements.modo) {
    controllerElements.modo.textContent = data.modo ?? '--';
  }
  if (controllerElements.rpm) {
    controllerElements.rpm.textContent = formatValue(data.rpm);
  }
  if (controllerElements.torque) {
    controllerElements.torque.textContent = formatValue(data.torque);
  }
  if (controllerElements.tempMotor) {
    controllerElements.tempMotor.textContent = formatValue(data.tempMotor);
  }
  if (controllerElements.tempBatt) {
    controllerElements.tempBatt.textContent = formatValue(data.tempBatt);
  }
}

/**
 * Atualiza indicador de status do servidor
 * @param {boolean} online - True se conectado
 */
export function setServerStatus(online) {
  if (!uiElements.statusServer) return;
  
  if (online) {
    uiElements.statusServer.classList.add('online');
    //uiElements.statusServer.innerHTML = '🟢 Servidor';
  } else {
    uiElements.statusServer.classList.remove('online');
    //uiElements.statusServer.innerHTML = '🔴 Servidor';
  }
}


/**
 * Atualiza indicador de status do ESP32
 * @param {boolean} connected - True se ESP32 conectado
 */

export function setEspStatus(connected) {
  if (!uiElements.statusEsp) return;
  
  if (connected) {
    uiElements.statusEsp.classList.add('online');
    uiElements.statusEsp.innerHTML = '🟢 ESP32';
  } else {
    uiElements.statusEsp.classList.remove('online');
    uiElements.statusEsp.innerHTML = '🔴 ESP32';
  }
}


/**
 * Alterna entre tema claro e escuro
 * @returns {string} Novo tema aplicado
 */
export function toggleTheme() {
  const body = document.body;
  const isDark = body.classList.contains('dark-theme');
  
  // Alterna classe no body
  _currentTheme = isDark ? 'light-theme' : 'dark-theme';
  body.className = _currentTheme;
  
  // Salva preferência no localStorage
  localStorage.setItem(UI.THEME_KEY, _currentTheme);
  
  // Atualiza ícone do botão
  if (uiElements.themeToggle) {
    uiElements.themeToggle.textContent = isDark ? '🌙' : '☀️';
  }
  
  console.log(`🎨 Tema alterado para: ${_currentTheme}`);
  return _currentTheme;
}

/**
 * Carrega tema salvo ou detecta preferência do sistema
 */
export function loadSavedTheme() {
  const saved = localStorage.getItem(UI.THEME_KEY);
  const prefersDark = window.matchMedia('(prefers-color-scheme: dark)').matches;
  
  _currentTheme = saved || (prefersDark ? 'dark-theme' : 'light-theme');
  document.body.className = _currentTheme;
  
  if (uiElements.themeToggle) {
    uiElements.themeToggle.textContent = _currentTheme === 'dark-theme' ? '☀️' : '🌙';
  }
  
  console.log(`🎨 Tema carregado: ${_currentTheme}`);
}

/**
 * Configura listeners para botões de download CSV
 */
export function setupDownloadButtons() {
  const { downloadCan, downloadDevice } = uiElements;
  
  if (downloadCan) {
    downloadCan.addEventListener('click', () => {
      window.location.href = '/api/export-can-data-csv';
      console.log('📥 Download CAN iniciado');
    });
  }
  
  if (downloadDevice) {
    downloadDevice.addEventListener('click', () => {
      window.location.href = '/api/export-vehicle-data-csv';
      console.log('📥 Download Veículo iniciado');
    });
  }
}

/**
 * Atualiza label de GPS com mensagem e cor
 * @param {string} message - Texto a exibir
 * @param {string} color - Cor CSS (opcional)
 */
export function updateGpsLabel(message, color = null) {
  if (!gpsElements.label) return;
  gpsElements.label.textContent = message;
  if (color) gpsElements.label.style.color = color;
}

/**
 * Aplica efeito de highlight em elemento por tempo limitado
 * @param {HTMLElement} element - Elemento a destacar
 * @param {number} duration - Duração em ms
 */
export function highlightElement(element, duration = UI.HIGHLIGHT_DURATION) {
  if (!element) return;
  element.classList.add('highlight');
  setTimeout(() => element.classList.remove('highlight'), duration);
}

/**
 * Exibe mensagem temporária em elemento
 * @param {HTMLElement} element - Elemento alvo
 * @param {string} message - Mensagem a exibir
 * @param {number} duration - Tempo em ms antes de restaurar
 */
export function showTemporaryMessage(element, message, duration = 2000) {
  if (!element) return;
  const original = element.textContent;
  element.textContent = message;
  setTimeout(() => { element.textContent = original; }, duration);
}

export default {
  updateBmsDisplay,
  updateControllerDisplay,
  setServerStatus,
  setEspStatus,
  toggleTheme,
  loadSavedTheme,
  setupDownloadButtons,
  updateGpsLabel,
  highlightElement,
  showTemporaryMessage
};
