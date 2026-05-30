/**
 * ============================================
 * 🗺️ map.js - Lógica do Mapa Leaflet
 * ============================================
 * Gerencia inicialização, atualização e interação
 * com o mapa de localização da moto.
 * 
 * @module map
 * @requires leaflet (global)
 * @requires ../config.js
 */

import { MAP } from './config.js';

// Estado privado do módulo
let _map = null;
let _marker = null;
let _lastCoords = null;

/**
 * Inicializa o mapa Leaflet no elemento especificado
 * @param {HTMLElement} container - Elemento DOM para renderizar o mapa
 * @returns {Object} Referência ao mapa e marcador
 */
export function initMap(container) {
  if (!container) {
    console.error('❌ Container do mapa não encontrado');
    return null;
  }
  
  // Cria o mapa com coordenadas iniciais
  _map = L.map(container).setView(MAP.INITIAL_COORDS, MAP.INITIAL_ZOOM);
  
  // Adiciona camada de tiles do OpenStreetMap
  L.tileLayer(MAP.TILE_LAYER, {
    attribution: MAP.TILE_ATTRIBUTION,
    tileSize: 512,
    zoomOffset: -1
  }).addTo(_map);
  
  // Cria ícone personalizado para a moto
  const bikeIcon = L.icon({
    iconUrl: MAP.MARKER_ICON.url,
    iconSize: MAP.MARKER_ICON.size,
    iconAnchor: MAP.MARKER_ICON.anchor,
    popupAnchor: MAP.MARKER_ICON.popupAnchor
  });
  
  // Adiciona marcador inicial com popup
  _marker = L.marker(MAP.INITIAL_COORDS, { 
    icon: bikeIcon, 
    title: 'Moto Voltz' 
  }).addTo(_map);
  
  _marker.bindPopup('📍 Moto Voltz<br><small>Em movimento</small>');
  
  console.log('🗺️ Mapa inicializado com sucesso');
  return { map: _map, marker: _marker };
}

/**
 * Atualiza a posição do marcador no mapa
 * @param {number} lat - Latitude
 * @param {number} lon - Longitude
 * @param {boolean} pan - Se deve mover a viewport do mapa (padrão: true)
 */
export function updateMapPosition(lat, lon, pan = true) {
  if (!_marker) {
    console.warn('⚠️ Marcador não inicializado. Chame initMap() primeiro.');
    return;
  }
  
  // Atualiza posição do marcador
  _marker.setLatLng([lat, lon]);
  
  // Opcionalmente move a viewport para a nova posição
  if (pan && _map) {
    _map.setView([lat, lon], _map.getZoom());
  }
  
  // Atualiza conteúdo do popup com coordenadas formatadas
  const latF = lat.toFixed(6);
  const lonF = lon.toFixed(6);
  _marker.getPopup().setContent(`
    📍 Moto Voltz<br>
    <small>Lat: ${latF}° | Lon: ${lonF}°</small>
  `);
  
  // Armazena última posição para referência
  _lastCoords = [lat, lon];
  
  console.log(`📍 Mapa atualizado: ${latF}, ${lonF}`);
}

/**
 * Obtém as últimas coordenadas registradas
 * @returns {Array<number>|null} [lat, lon] ou null se não houver
 */
export function getLastCoords() {
  return _lastCoords ? [..._lastCoords] : null;
}

/**
 * Centraliza o mapa em uma posição específica
 * @param {number} lat - Latitude
 * @param {number} lon - Longitude
 * @param {number} zoom - Nível de zoom (opcional)
 */
export function centerMap(lat, lon, zoom = null) {
  if (!_map) return;
  const targetZoom = zoom !== null ? zoom : _map.getZoom();
  _map.setView([lat, lon], targetZoom);
}

/**
 * Adiciona um círculo de precisão ao redor do marcador
 * @param {number} accuracy - Raio da precisão em metros
 */
export function addAccuracyCircle(accuracy) {
  if (!_marker || !_map) return;
  
  // Remove círculo anterior se existir
  if (_marker._accuracyCircle) {
    _map.removeLayer(_marker._accuracyCircle);
  }
  
  // Cria novo círculo
  _marker._accuracyCircle = L.circle(_marker.getLatLng(), {
    radius: accuracy,
    color: '#3498db',
    fillColor: '#3498db',
    fillOpacity: 0.15,
    weight: 1
  }).addTo(_map);
}

/**
 * Remove o círculo de precisão
 */
export function removeAccuracyCircle() {
  if (_marker?._accuracyCircle && _map) {
    _map.removeLayer(_marker._accuracyCircle);
    delete _marker._accuracyCircle;
  }
}

export default {
  initMap,
  updateMapPosition,
  getLastCoords,
  centerMap,
  addAccuracyCircle,
  removeAccuracyCircle
};
