/**
 * ============================================
 * 🚀 main.js - Ponto de Entrada do Dashboard
 * ============================================
 * Orquestra inicialização dos módulos,
 * configuração de eventos e loops de atualização.
 * 
 * @module main
 * @requires ./config.js
 * @requires ./dom-elements.js
 * @requires ./utils.js
 * @requires ./map.js
 * @requires ./gps.js
 * @requires ./api.js
 * @requires ./websocket.js
 * @requires ./can-table.js
 * @requires ./ui.js
 */

// 🔹 Imports de módulos
import { GPS, UI } from './config.js';
import { validateElements, tableElements, gpsElements } from './dom-elements.js';
import { debounce } from './utils.js';
import { initMap, updateMapPosition } from './map.js';
import * as gps from './gps.js';
import { fetchCanFrames, loadVehicleTable, fetchDecodedData } from './api.js';
import { connect as connectWebSocket } from './websocket.js';
import { updateCanTable } from './can-table.js';
import { loadSavedTheme, toggleTheme, setupDownloadButtons, updateGpsLabel } from './ui.js';
import { updateBmsDisplay, updateControllerDisplay } from './ui.js';
    
// 🔹 Estado global (apenas para referência entre módulos)
const AppState = {
  isInitialized: false,
  lastUpdate: 0
};

/**
 * Inicializa todos os módulos do dashboard
 */
async function initialize() {
  console.log('🚀 Inicializando Dashboard Moto Voltz...');
  
  // 🔹 1. Valida elementos do DOM
  if (!validateElements()) {
    console.error('❌ Falha crítica: elementos do DOM não encontrados');
    return;
  }
  
  // 🔹 2. Carrega tema salvo
  loadSavedTheme();
  
  // 🔹 3. Inicializa mapa
  const { mapElement } = await import('./dom-elements.js');
  initMap(mapElement);
  
  // 🔹 4. Configura UI
  setupDownloadButtons();
  
  // 🔹 5. Configura botão de tema
  const { uiElements } = await import('./dom-elements.js');
  uiElements.themeToggle?.addEventListener('click', toggleTheme);
  
  // 🔹 6. Configura GPS
  gps.setUIElements(gpsElements);
  gpsElements.btn?.addEventListener('click', gps.toggleTracking);
  
  // 🔹 7. Conecta WebSocket
  connectWebSocket();
  
  // 🔹 8. Carrega dados iniciais
  await refreshAllData();
  
  // 🔹 9. Configura loops de atualização
  setupAutoRefresh();
  
  AppState.isInitialized = true;
  console.log('✅ Dashboard inicializado com sucesso');
}

/**
 * Atualiza todos os dados do dashboard (API polling)
 */
async function refreshAllData() {
  try {

    // 🔹 Busca dados decodificados
    const records = await fetchDecodedData();
    
    // 🔹 Se houver dados, atualiza a UI com o registro mais recente
    if (records?.length > 0) {
      const latest = records[records.length - 1];
      
      if (latest.battery) {
        updateBmsDisplay({
          current: latest.battery.current,
          voltage: latest.battery.voltage,
          soc: latest.battery.soc,
          soh: latest.battery.soh,
          temperature: latest.battery.temperature
        });
      }
      
      if (latest.motor) {
        updateControllerDisplay({
          modo: latest.motor.modo,
          rpm: latest.motor.rpm,
          torque: latest.motor.torque,
          tempMotor: latest.motor.motorTemp,
          tempBatt: latest.motor.controlTemp
        });
      }
    }
    
    // Atualiza tabela CAN (frames brutos)
    const canFrames = await fetchCanFrames();
    updateCanTable(tableElements.canBody, canFrames, false); // false = replace mode
    
    // Atualiza tabela de veículo (dados decodificados)
    await loadVehicleTable(tableElements.vehicleBody);
    
    AppState.lastUpdate = Date.now();
    
  } catch (error) {
    console.error('❌ Erro ao atualizar dados:', error);
    updateGpsLabel('⚠️ Erro ao carregar dados', '#f44336');
  }
}

/**
 * Configura intervalos automáticos de atualização
 * Usa debounce para evitar sobrecarga
 */
function setupAutoRefresh() {
  // Debounce para evitar múltiplas chamadas simultâneas
  const debouncedRefresh = debounce(refreshAllData, 500);
  
  // Intervalo principal de polling (configurável)
  setInterval(() => {
    if (AppState.isInitialized) {
      debouncedRefresh();
    }
  }, UI.FETCH_INTERVAL);
  
  console.log(`🔄 Auto-refresh configurado: ${UI.FETCH_INTERVAL}ms`);
}

/**
 * Handler para quando o DOM estiver completamente carregado
 */
function onDomReady() {
  // Aguarda Leaflet carregar (script externo)
  if (typeof L === 'undefined') {
    console.warn('⏳ Aguardando Leaflet carregar...');
    setTimeout(onDomReady, 100);
    return;
  }
  
  // Inicializa aplicação
  initialize();
}

// 🔹 Event Listeners Globais

// DOMContentLoaded: ponto de entrada principal
document.addEventListener('DOMContentLoaded', onDomReady);

// Page visibility: pausa atualizações quando aba não está visível (otimização)
document.addEventListener('visibilitychange', () => {
  if (document.hidden) {
    console.log('👁️ Dashboard em background - reduzindo atualizações');
    // Opcional: aumentar intervalo de polling
  } else {
    console.log('👁️ Dashboard em foco - retomando atualizações');
    // Opcional: forçar refresh imediato
    if (AppState.isInitialized) {
      refreshAllData();
    }
  }
});

// Error handling global para debug
window.addEventListener('error', (event) => {
  console.error('💥 Erro global não tratado:', {
    message: event.message,
    source: event.filename,
    line: event.lineno,
    column: event.colno
  });
});

// Unhandled promise rejections
window.addEventListener('unhandledrejection', (event) => {
  console.error('💥 Promise rejeitada não tratada:', event.reason);
});

// Exporta para debug no console (opcional)
if (import.meta.env?.DEV || window.location.hostname === 'localhost') {
  window.Dashboard = {
    AppState,
    refreshAllData,
    gps,
    ...await import('./api.js'),
    ...await import('./map.js')
  };
  console.log('🛠️ Dashboard exposto no window para debug');
}

export default { initialize, refreshAllData, AppState };
