/**
 * ============================================
 * 🛠️ utils.js - Funções Utilitárias
 * ============================================
 * Funções auxiliares reutilizáveis para formatação,
 * validação e helpers gerais do dashboard.
 * 
 * @module utils
 */

/**
 * Formata um timestamp para o formato pt-BR
 * @param {string|number|Date} ts - Timestamp em qualquer formato
 * @returns {string} Data/hora formatada ou '—' se inválido
 * 
 * @example
 * formatTimestamp('2024-01-15T14:30:25') 
 * // Retorna: "15/01/2024, 14:30:25"
 */
export function formatTimestamp(ts) {
  if (!ts) return '—';
  
  const date = typeof ts === 'number' ? new Date(ts) : new Date(ts);
  
  // Valida se a data é válida
  if (isNaN(date.getTime())) return '—';
  
  return date.toLocaleString('pt-BR', {
    day: '2-digit',
    month: '2-digit',
    year: 'numeric',
    hour: '2-digit',
    minute: '2-digit',
    second: '2-digit',
    fractionalSecondDigits: 3
  }).replaceAll(',', '.');
}

/**
 * Formata apenas a hora (HH:MM:SS) de um timestamp
 * @param {string|number|Date} date - Timestamp
 * @returns {string} Hora formatada
 */
export function formatTimeOnly(date) {
  const d = new Date(date);
  return d.toLocaleTimeString('pt-BR');
}

/**
 * Formata valor numérico com casas decimais e fallback
 * @param {*} val - Valor a ser formatado
 * @param {number} decimals - Número de casas decimais (padrão: 2)
 * @returns {string} Valor formatado ou '—' se inválido
 * 
 * @example
 * formatValue(12.345, 2)  // Retorna: "12.35"
 * formatValue(null)       // Retorna: "—"
 */
export function formatValue(val, decimals = 2) {
  if (val == null || val === '--' || val === '') return '—';
  
  const num = Number(val);
  return isNaN(num) ? '—' : num.toFixed(decimals);
}

/**
 * Extrai e formata coordenadas de localização GeoJSON
 * @param {Object} record - Objeto contendo campo location
 * @returns {Object|null} Objeto com dados formatados ou null
 * 
 * @example
 * // Entrada: { location: { coordinates: [-34.95, -8.05] } }
 * // Saída: { display: "-8.050000, -34.950000", lat: -8.05, lon: -34.95, ... }
 */
export function formatLocation(record) {
  const coords = record?.location?.coordinates;
  
  // Valida se é array com 2 elementos [lon, lat] (padrão GeoJSON)
  if (!Array.isArray(coords) || coords.length !== 2) return null;
  
  const [lon, lat] = coords;
  const latF = Number(lat).toFixed(6);
  const lonF = Number(lon).toFixed(6);
  
  return {
    display: `${latF}, ${lonF}`,
    lat: Number(lat),
    lon: Number(lon),
    title: `Lat: ${latF}° | Lon: ${lonF}°`
  };
}

/**
 * Debounce: limita a frequência de execução de uma função
 * @param {Function} func - Função a ser executada
 * @param {number} wait - Tempo de espera em ms
 * @returns {Function} Função com debounce aplicado
 */
export function debounce(func, wait) {
  let timeout;
  return function executedFunction(...args) {
    const later = () => {
      clearTimeout(timeout);
      func(...args);
    };
    clearTimeout(timeout);
    timeout = setTimeout(later, wait);
  };
}

/**
 * Throttle: executa função no máximo uma vez por intervalo
 * @param {Function} func - Função a ser executada
 * @param {number} limit - Intervalo mínimo entre execuções (ms)
 * @returns {Function} Função com throttle aplicado
 */
export function throttle(func, limit) {
  let inThrottle;
  return function(...args) {
    if (!inThrottle) {
      func.apply(this, args);
      inThrottle = true;
      setTimeout(() => inThrottle = false, limit);
    }
  };
}

/**
 * Clona objeto profundamente (shallow clone para objetos simples)
 * @param {Object} obj - Objeto a ser clonado
 * @returns {Object} Cópia do objeto
 */
export function clone(obj) {
  return JSON.parse(JSON.stringify(obj));
}

/**
 * Verifica se um valor é um número válido
 * @param {*} val - Valor a verificar
 * @returns {boolean} True se for número finito
 */
export function isValidNumber(val) {
  return typeof val === 'number' && isFinite(val);
}

export default {
  formatTimestamp,
  formatTimeOnly,
  formatValue,
  formatLocation,
  debounce,
  throttle,
  clone,
  isValidNumber
};
