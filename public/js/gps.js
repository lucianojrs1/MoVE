/**
 * ============================================
 * 🛰️ gps.js - Controle de Geolocalização
 * ============================================
 * Gerencia captura, envio e exibição de dados GPS
 * do dispositivo móvel do usuário.
 * 
 * @module gps
 * @requires ../config.js
 * @requires ../utils.js
 * @requires ./api.js
 * @requires ./map.js
 */

import { GPS, API } from './config.js';
import { formatValue } from './utils.js';
import { sendLocationToApi } from './api.js';
import { updateMapPosition, addAccuracyCircle, removeAccuracyCircle } from './map.js';

// Estado privado
let _watchId = null;
let _isTracking = false;
let _uiElements = null;

/**
 * Configura referências aos elementos de UI do GPS
 * @param {Object} elements - Objeto com elementos do DOM
 */
export function setUIElements(elements) {
  _uiElements = elements;
}

/**
 * Envia localização para o servidor via API
 * @param {number} lat - Latitude
 * @param {number} lon - Longitude  
 * @param {number} accuracy - Precisão em metros
 * @returns {Promise<boolean>} Sucesso da operação
 */
export async function sendLocation(coords) {
  try {
    const success = await sendLocationToApi(coords, GPS.DEVICE_ID);
    updateMapPosition(coords.latitude, coords.longitude);
     addAccuracyCircle(coords.accuracy);
    if (success && _uiElements?.label) {
      console.log('✅ Localização enviada:', coords);
      //_uiElements.label.textContent = `✅ Enviado: ${coords.latitude.toFixed(5)}, ${coords.longitude.toFixed(5)}`;
      _uiElements.label.style.color = '#4CAF50';
    }
    return success;
  } catch (error) {
    console.error('❌ Erro ao enviar GPS:', error);
    if (_uiElements?.label) {
      _uiElements.label.textContent = '🌐 Erro de conexão';
      _uiElements.label.style.color = '#f44336';
    }
    return false;
  }
}

/**
 * Callback de sucesso para geolocalização
 * @param {GeolocationPosition} position - Objeto de posição do navegador
 */

function onGpsSuccess(position) {
  // 🔹 Extrai TODOS os campos disponíveis no objeto coords
  const {
    latitude,
    longitude,
    accuracy,
    altitude,           // metros (m) acima do elipsoide WGS84
    altitudeAccuracy,   // metros (m) - incerteza da altitude
    heading,            // graus (°) - 0=Norte, 90=Leste, 180=Sul, 270=Oeste
    speed               // metros/segundo (m/s)
  } = position.coords;

  const timestamp = position.timestamp; // ms desde epoch

  // 🔹 Formata para exibição segura (lida com null)
  const display = {
    lat: latitude.toFixed(5),
    lon: longitude.toFixed(5),
    accuracy: Math.round(accuracy),
    altitude: altitude !== null ? `${altitude.toFixed(1)}m` : '—',
    altAcc: altitudeAccuracy !== null ? `±${Math.round(altitudeAccuracy)}m` : '—',
    heading: heading !== null ? `${Math.round(heading)}°` : '—',
    // Converte m/s para km/h para leitura humana
    speed: speed !== null ? `${(speed * 3.6).toFixed(1)} km/h` : '0 km/h',
    time: new Date(timestamp).toLocaleTimeString()
  };

  // 🔹 Atualiza label de status
  if (_uiElements?.label) {
    _uiElements.label.innerHTML = `
      📡 ${display.lat}, ${display.lon}<br>
      <small>
        🕒 ${display.time} | 🏔️ Alt: ${display.altitude} 
        | 🧭 Dir: ${display.heading} | 🚀 Vel: ${display.speed} 
        | 🎯 ±${display.accuracy}m
      </small>
    `;
    _uiElements.label.style.color = '#2196F3';
  }

  // 🔹 Atualiza mapa e envia para API
  updateMapPosition(latitude, longitude);
  addAccuracyCircle(accuracy);

  // Envia objeto completo (nova assinatura)
  sendLocation({
    latitude, longitude, accuracy,
    altitude, altitudeAccuracy, heading, speed,
    timestamp
  });
}


/**
 * Callback de erro para geolocalização
 * @param {GeolocationPositionError} error - Objeto de erro
 */
function onGpsError(error) {
  let message = '⚠️ GPS não disponível';
  
  // Mapeia códigos de erro para mensagens amigáveis
  switch (error.code) {
    case error.PERMISSION_DENIED:
      message = '❌ Permissão de localização negada';
      break;
    case error.POSITION_UNAVAILABLE:
      message = '❌ Sinal de GPS indisponível';
      break;
    case error.TIMEOUT:
      message = '⏱️ Tempo limite para obter localização';
      break;
    default:
      message = `❌ Erro desconhecido: ${error.message}`;
  }
  
  console.error('🛰️ Erro de geolocalização:', message);
  
  if (_uiElements?.label) {
    _uiElements.label.textContent = message;
    _uiElements.label.style.color = '#f44336';
  }
  
  // Para rastreamento em caso de erro crítico
  if (error.code !== error.TIMEOUT) {
    stopTracking();
  }
}

/**
 * Inicia rastreamento contínuo de GPS
 * @returns {boolean} True se iniciado com sucesso
 */
export function startTracking() {
  // Verifica suporte do navegador
  if (!navigator.geolocation) {
    if (_uiElements?.label) {
      _uiElements.label.textContent = '❌ Navegador sem suporte a GPS';
      _uiElements.label.style.color = '#f44336';
    }
    alert('Seu navegador não suporta geolocalização. Use Chrome, Firefox ou Edge atualizado.');
    return false;
  }
  
  // Atualiza estado e UI
  _isTracking = true;
  if (_uiElements?.btn) {
    _uiElements.btn.disabled = false;
    _uiElements.btn.textContent = '⏹️ Parar GPS';
  }
  if (_uiElements?.label) {
    _uiElements.label.textContent = '📡 Aguardando sinal GPS...';
    _uiElements.label.style.color = '#ff9800';
  }
  
  // Inicia watchPosition para atualizações contínuas
  _watchId = navigator.geolocation.watchPosition(
    onGpsSuccess,
    onGpsError,
    GPS.WATCH_OPTIONS
  );
  
  console.log('🛰️ Rastreamento GPS iniciado');
  return true;
}

/**
 * Para o rastreamento de GPS
 */
export function stopTracking() {
  if (_watchId !== null) {
    navigator.geolocation.clearWatch(_watchId);
    _watchId = null;
  }
  
  // Limpa indicador de precisão no mapa
  removeAccuracyCircle();
  
  // Atualiza estado e UI
  _isTracking = false;
  if (_uiElements?.btn) {
    _uiElements.btn.disabled = false;
    _uiElements.btn.textContent = '🔎 Buscar GPS 📍';
  }
  if (_uiElements?.label) {
    _uiElements.label.textContent = 'GPS parado';
    _uiElements.label.style.color = '#666';
  }
  
  console.log('🛰️ Rastreamento GPS parado');
}

/**
 * Alterna entre iniciar/parar rastreamento
 * @returns {boolean} Novo estado do rastreamento
 */
export function toggleTracking() {
  if (_isTracking) {
    stopTracking();
    return false;
  } else {
    return startTracking();
  }
}

/**
 * Obtém posição única (sem rastreamento contínuo)
 * @param {Function} onSuccess - Callback com posição
 * @param {Function} onError - Callback com erro
 */
export function getCurrentPosition(onSuccess, onError) {
  if (!navigator.geolocation) {
    onError?.(new Error('Geolocalização não suportada'));
    return;
  }
  
  navigator.geolocation.getCurrentPosition(
    (pos) => {
      const { latitude, longitude, accuracy } = pos.coords;
      onSuccess?.({ latitude, longitude, accuracy });
    },
    (err) => onError?.(err),
    { ...GPS.WATCH_OPTIONS, maximumAge: 0 } // Força nova leitura
  );
}

/**
 * Verifica se o rastreamento está ativo
 * @returns {boolean}
 */
export function isTracking() {
  return _isTracking;
}

export default {
  setUIElements,
  sendLocation,
  startTracking,
  stopTracking,
  toggleTracking,
  getCurrentPosition,
  isTracking
};
