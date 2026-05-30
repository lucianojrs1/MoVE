/**
 * ============================================
 * 🔌 api.js - Comunicação com Backend
 * ============================================
 * Funções para requisições HTTP à API REST
 * e envio de dados para o servidor.
 * 
 * @module api
 * @requires ../config.js
 * @requires ../utils.js
 */

import { API } from './config.js';
import { formatValue, formatLocation } from './utils.js';

// Cache opcional para evitar requisições repetidas em curto intervalo
const _cache = {
  deviceData: null,
  lastFetch: 0,
  TTL: 1000  // 1 segundo de cache
};

/**
 * Busca dados decodificados do dispositivo (BMS, motor, GPS)
 * @param {boolean} useCache - Se pode usar dados em cache
 * @returns {Promise<Array>} Lista de registros do dispositivo
 */
export async function fetchDecodedData(useCache = true) {
  const now = Date.now();
  
  // Retorna cache se válido
  if (useCache && _cache.deviceData && (now - _cache.lastFetch) < _cache.TTL) {
    return _cache.deviceData;
  }
  
  try {
    const response = await fetch(API.DEVICE_DATA);
    
    if (!response.ok) {
      throw new Error(`HTTP ${response.status}: ${response.statusText}`);
    }
    
    const data = await response.json();
   
    // Atualiza cache
    _cache.deviceData = Array.isArray(data) ? data : [];
    _cache.lastFetch = now;
    return _cache.deviceData;
    
  } catch (error) {
    console.error('❌ Erro ao buscar dados decodificados:', error);
    throw error;
  }
}

/**
 * Busca frames CAN brutos recentes
 * @param {number} limit - Quantidade máxima de frames (padrão: 50)
 * @returns {Promise<Array>} Lista de frames CAN
 */
export async function fetchCanFrames(limit = 50) {
  try {
    const response = await fetch(`${API.CAN_DATA}?limit=${limit}`);
    
    if (!response.ok) {
      throw new Error(`HTTP ${response.status}: ${response.statusText}`);
    }
    
    return await response.json();
    
  } catch (error) {
    console.error('❌ Erro ao buscar frames CAN:', error);
    throw error;
  }
}

/**
 * Envia coordenadas GPS para o servidor
 * @param {number} lat - Latitude
 * @param {number} lon - Longitude
 * @param {number} accuracy - Precisão em metros
 * @param {string} deviceId - Identificador do dispositivo
 * @returns {Promise<boolean>} True se enviado com sucesso
 */
export async function sendLocationToApi(coords, deviceId) {
  try {
    const response = await fetch(`${API.LOCATION}`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        //deviceId: deviceId,
        location: {
          type: "Point",
          coordinates: [coords.longitude, coords.latitude]
        },
        latitude: coords.latitude,
        longitude: coords.longitude,
        accuracy: coords.accuracy,
        speed: coords.speed,
        altitude: coords.altitude,
        altitudeAccuracy: coords.altitudeAccuracy,
        heading: coords.heading,
        
      })
    });

    if (!response.ok) {
      const errorText = await response.text().catch(() => '');
      throw new Error(`HTTP ${response.status}: ${errorText.substring(0, 100)}`);
    }

    
    return true;

  } catch (error) {
    console.error('❌ Erro ao enviar localização:', error);
    return false;
  }
}

/**
 * Carrega e renderiza tabela de dados do veículo
 * @param {HTMLElement} tbody - Elemento tbody da tabela
 * @param {number} maxRows - Máximo de linhas a exibir
 * @returns {Promise<Array>} Dados carregados
 */
export async function loadVehicleTable(tbody, maxRows = 20) {
  try {
    // 🔹 1. Busca dados da API
    const records = await fetchDecodedData();
    
    
    // 🔹 2. Valida e limita registros
    const data = Array.isArray(records) ? records.slice(0, maxRows) : [];
    
    // 🔹 3. Caso vazio: exibe mensagem
    if (data.length === 0) {
      tbody.innerHTML = `
        <tr id="empty-row">
          <td colspan="10" class="empty">Nenhum dado de veículo recebido.</td>
        </tr>
      `;
      return [];
    }
    
    // 🔹 4. Remove linha "vazio" se existir
    const emptyRow = tbody.querySelector('#empty-row');
    if (emptyRow) emptyRow.remove();

    // 🔹 5. Gera HTML das linhas (mais eficiente que append individual)
    const rowsHTML = data.map(record => {
      const loc = formatLocation(record);
      const locationCell = loc 
        ? `<span title="${loc.title}">${loc.display}</span>` 
        : '—';

      return `
        <tr class="highlight" data-timestamp="${record.timestamp || ''}">
          <td>${formatTimestamp(record.timestamp)}</td>
          <td>${record.motor?.modo ?? '—'}</td>
          <td>${formatValue(record.motor?.rpm)}</td>
          <td>${formatValue(record.motor?.torque)}</td>
          <td>${formatValue(record.battery?.soc)}</td>
          <td>${formatValue(record.motor?.motorTemp)}</td>
          <td>${formatValue(record.battery?.temperature)}</td>
          <td>${formatValue(record.battery?.voltage, 3)}</td>
          <td>${formatValue(record.battery?.current)}</td>
          <td>${locationCell}</td>
          <td>${formatValue(record.accuracy)}</td>
          <td>${formatValue(record.speed)}</td>
          <td>${formatValue(record.altitude)}</td>
          <td>${formatValue(record.altitudeAccuracy)}</td>
          <td>${formatValue(record.heading)}</td>
        </tr>
      `;
    }).join('');

    // 🔹 6. Injeta tudo de uma vez (menos reflows no DOM)
    tbody.innerHTML = rowsHTML;

    // 🔹 7. Remove efeito highlight após animação
    setTimeout(() => {
      tbody.querySelectorAll('.highlight').forEach(tr => 
        tr.classList.remove('highlight')
      );
    }, 500);

    return data;

  } catch (error) {
    console.error('❌ Erro ao carregar tabela do veículo:', error);
    
    // Exibe erro na tabela
    tbody.innerHTML = `
      <tr>
        <td colspan="10" class="empty" style="color: var(--danger, #dc3545);">
          ⚠️ Erro ao carregar: ${error.message}
        </td>
      </tr>
    `;
    
    return [];
  }
}

/**
 * Inicia download de arquivo CSV da API
 * @param {string} endpoint - Endpoint de exportação
 * @param {string} filename - Nome do arquivo para download
 */
export function downloadCsv(endpoint, filename) {
  const link = document.createElement('a');
  link.href = endpoint;
  link.download = `${filename}-${Date.now()}.csv`;
  document.body.appendChild(link);
  link.click();
  document.body.removeChild(link);
  
  console.log(`📥 Download iniciado: ${filename}`);
}

// Importa formatTimestamp de utils para usar em loadVehicleTable
import { formatTimestamp } from './utils.js';

export default {
  fetchDecodedData,
  fetchCanFrames,
  sendLocationToApi,
  loadVehicleTable,
  downloadCsv
};
