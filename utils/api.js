// src/utils/api.js

const API_URL = process.env.API_URL;
/**
 * Cria um novo registro de veículo (VehicleData)
 * @param {Object} data - Dados do veículo
 */
export async function createVehicleData(data) {
  const response = await fetch(`${API_URL}/api/device`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(data)
  });
  if (!response.ok) throw new Error(`Erro ao criar dados do veículo: ${response.statusText}`);
  return await response.json();
}

/**
 * Busca todos os registros de um dispositivo específico
 * @param {string} deviceId
 * @param {number} [limit=20]
 */
export async function getVehicleDataByDeviceId(deviceId, limit = 20) {
  const response = await fetch(`${API_URL}/api/device/${deviceId}?limit=${limit}`);
  if (!response.ok) throw new Error(`Erro ao buscar dados do dispositivo: ${response.statusText}`);
  return await response.json();
}

/**
 * Adiciona um ou mais frames CAN a um dispositivo
 * @param {string} deviceId
 * @param {Object|Object[]} canMessages - Frame único ou array de frames
 */
export async function addCanMessage( canMessages) {
  const response = await fetch(`${API_URL}/api/can`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(canMessages)
  });
  if (!response.ok) throw new Error(`Erro ao adicionar mensagem CAN: ${response.statusText}`);
  return await response.json();
}

/**
 * Retorna os últimos N frames CAN brutos
 * @param {number} [limit=50]
 * @param {string} [deviceId] - Opcional: filtra por deviceId
 */
export async function getRecentCanData(limit = 50, deviceId = null) {
  const params = new URLSearchParams();
  params.append('limit', limit);
  if (deviceId) params.append('deviceId', deviceId);

  const url = `${API_URL}/api/can-data${params.toString() ? '?' + params.toString() : ''}`;
  const response = await fetch(url);
  if (!response.ok) throw new Error(`Erro ao buscar dados CAN: ${response.statusText}`);
  return await response.json();
}

/**
 * Retorna os últimos N frames CAN decodificados
 * @param {number} [limit=50]
 */
export async function getDecodedCanData(limit = 50) {
  const response = await fetch(`${API_URL}/api/decoded-can-data?limit=${limit}`);
  if (!response.ok) throw new Error(`Erro ao buscar dados CAN decodificados: ${response.statusText}`);
  return await response.json();
}

/**
 * Baixa todos os dados CAN como CSV
 * @param {string} [deviceId] - Opcional
 */
export function downloadCanDataAsCsv(deviceId = null) {
  const url = deviceId
    ? `${API_URL}/api/export-can-data-csv?deviceId=${deviceId}`
    : `${API_URL}/api/export-can-data-csv`;

  const link = document.createElement('a');
  link.href = url;
  link.download = `can-data-${Date.now()}.csv`;
  link.click();
}

/**
 * Atualiza a localização de um dispositivo (ex: do GPS do celular)
 * @param {string} deviceId
 * @param {Object} locationData - { latitude, longitude, accuracy, timestamp }
 */
export async function updateDeviceLocation(deviceId, locationData) {
  const response = await fetch(`${API_URL}/api/device/${deviceId}/location`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(locationData)
  });
  if (!response.ok) throw new Error(`Erro ao atualizar localização: ${response.statusText}`);
  return await response.json();
}