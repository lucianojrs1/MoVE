/**
 * ============================================
 * 📋 can-table.js - Tabela de Frames CAN
 * ============================================
 * Funções para renderização e atualização
 * da tabela de frames CAN brutos.
 * 
 * @module canTable
 * @requires ../config.js
 * @requires ../utils.js
 */

import { UI } from './config.js';
import { formatTimestamp, formatTimeOnly } from './utils.js';

// Cache do último frame para evitar duplicatas visuais
let _lastFrameHash = null;

/**
 * Gera hash simples para identificar frame único
 * @param {Object} frame - Objeto do frame CAN
 * @returns {string} Hash identificador
 */
function generateFrameHash(frame) {
  return `${frame.timestamp}-${frame.canId}-${Array.isArray(frame.data) ? frame.data.join('') : frame.data}`;
}

/**
 * Cria elemento <tr> para um frame CAN
 * @param {Object} frame - Dados do frame: { timestamp, canId, data }
 * @returns {HTMLTableRowElement} Linha da tabela
 */
function createCanRow(frame) {
  const tr = document.createElement('tr');
  tr.classList.add('highlight');
  
  // Formata ID CAN para hexadecimal maiúsculo
  const canIdHex = `0x${frame.canId.toString(16).toUpperCase()}`;
  
  // Formata dados: array para string separada por vírgula
  const dataDisplay = Array.isArray(frame.data) 
    ? frame.data.map(b => b.toString(16).padStart(2, '0').toUpperCase()).join(', ')
    : frame.data;
  
  tr.innerHTML = `
    <td>${formatTimestamp(frame.timestamp)}</td>
    <td>${canIdHex}</td>
    <td>${dataDisplay}</td>
  `;
  
  return tr;
}

/**
 * Atualiza tabela CAN com lista de frames (batch update)
 * @param {HTMLElement} tbody - Elemento tbody da tabela
 * @param {Array} messages - Lista de frames CAN
 * @param {boolean} prepend - Se adiciona no topo (true) ou substitui (false)
 */
export function updateCanTable(tbody, messages, prepend = true) {
  if (!tbody) {
    console.warn('⚠️ Elemento tbody não fornecido');
    return;
  }
  
  // 🔹 Caso: sem mensagens
  if (!messages || messages.length === 0) {
    tbody.innerHTML = `
      <tr>
        <td colspan="3" class="empty">Nenhum frame CAN recebido.</td>
      </tr>
    `;
    return;
  }
  
  // 🔹 Caso: modo prepend (adiciona novos no topo, mantém antigos)
  if (prepend) {
    messages.forEach(frame => {
      // Evita duplicatas consecutivas
      const hash = generateFrameHash(frame);
      if (hash === _lastFrameHash) return;
      _lastFrameHash = hash;
      
      const tr = createCanRow(frame);
      tbody.prepend(tr);
      
      // Remove highlight após animação
      setTimeout(() => tr.classList.remove('highlight'), UI.HIGHLIGHT_DURATION);
    });
    
    // 🔹 Limita número de linhas para performance
    while (tbody.children.length > UI.MAX_CAN_ROWS) {
      tbody.removeChild(tbody.lastChild);
    }
    
  } 
  // 🔹 Caso: modo replace (substitui todo o conteúdo)
  else {
    tbody.innerHTML = '';
    messages.forEach(frame => {
      const tr = createCanRow(frame);
      tbody.appendChild(tr);
      setTimeout(() => tr.classList.remove('highlight'), UI.HIGHLIGHT_DURATION);
    });
  }
}

/**
 * Atualiza tabela com frame único em tempo real (via WebSocket)
 * @param {Object} frame - Frame CAN individual
 */
export function updateCanTableRealtime(frame) {
  const tbody = document.querySelector('#can-body');
  if (!tbody) return;
  
  // Remove mensagem "vazio" se existir
  const emptyRow = tbody.querySelector('.empty');
  if (emptyRow) emptyRow.remove();
  
  // Cria e insere nova linha
  const tr = createCanRow(frame);
  tbody.prepend(tr);
  setTimeout(() => tr.classList.remove('highlight'), UI.HIGHLIGHT_DURATION);
  
  // Mantém limite de linhas
  while (tbody.children.length > UI.MAX_CAN_ROWS) {
    tbody.removeChild(tbody.lastChild);
  }
}

/**
 * Limpa completamente a tabela CAN
 * @param {HTMLElement} tbody - Elemento tbody
 * @param {string} emptyMessage - Mensagem para exibir quando vazio
 */
export function clearCanTable(tbody, emptyMessage = 'Aguardando frames...') {
  if (!tbody) return;
  tbody.innerHTML = `
    <tr>
      <td colspan="3" class="empty">${emptyMessage}</td>
    </tr>
  `;
  _lastFrameHash = null;
}

/**
 * Exporta frames CAN como texto formatado (para CSV/custom)
 * @param {Array} frames - Lista de frames
 * @returns {string} Texto CSV formatado
 */
export function exportCanFramesAsCsv(frames) {
  const headers = ['Timestamp', 'CAN_ID_Hex', 'Data_Hex'];
  const rows = frames.map(f => [
    new Date(f.timestamp).toISOString(),
    `0x${f.canId.toString(16).toUpperCase()}`,
    Array.isArray(f.data) ? f.data.map(b => b.toString(16).padStart(2,'0').toUpperCase()).join(' ') : f.data
  ]);
  
  return [headers, ...rows].map(row => row.join(',')).join('\n');
}

export default {
  updateCanTable,
  updateCanTableRealtime,
  clearCanTable,
  exportCanFramesAsCsv
};
