/**
 * ============================================
 * 🔗 websocket.js - Conexão WebSocket
 * ============================================
 * Gerencia conexão em tempo real com o servidor
 * para recebimento de dados do ESP32 e status.
 * 
 * @module websocket
 * @requires ../config.js
 * @requires ./can-table.js
 * @requires ./ui.js
 */

import { WS } from './config.js';
import { updateCanTableRealtime } from './can-table.js';
import { updateBmsDisplay, updateControllerDisplay, setServerStatus, setEspStatus } from './ui.js';

// Estado da conexão
let _ws = null;
let _reconnectAttempts = 0;
const MAX_RECONNECT_ATTEMPTS = 10;

/**
 * Processa mensagem recebida via WebSocket
 * @param {string|Object} data - Dados brutos ou parseados
 */
function handleMessage(data) {
  try {
    // 🔹 Mensagens de status do ESP32 (texto simples)
    if (typeof data === 'string') {
      if (data.includes('ESP32 Conectado')) {
        setEspStatus(true);
        console.log('✅', data);
        return;
      }
      if (data.includes('ESP32 Desconectado')) {
        setEspStatus(false);
        console.log('⚠️', data);
        return;
      }
      console.log('📨 Mensagem de texto:', data);
      return;
    }

    // 🔹 Valida objeto JSON
    if (typeof data !== 'object' || data === null) {
      console.warn('⚠️ Dados inválidos recebidos:', data);
      return;
    }

    // 🔹 Roteia por tipo de mensagem
    switch (data.type) {
      case 'battery':
        // Atualiza display do BMS com dados decodificados
        if (data.decoded) {
          const { current, voltage, soc, soh, temperature } = data.decoded;
          updateBmsDisplay({ current, voltage, soc, soh, temperature });
        }
        break;
        
      case 'motorController':
        // Atualiza display do controller/motor
        if (data.decoded) {
          const { motorSpeedRpm, motorTorque, motorTemperature, controllerTemperature, modo } = data.decoded;
          updateControllerDisplay({ 
            modo, 
            rpm: motorSpeedRpm, 
            torque: motorTorque, 
            tempMotor: motorTemperature, 
            tempBatt: controllerTemperature 
          });
        }
        break;
        
      case 'canFrame':
        // Atualiza tabela CAN em tempo real com frame bruto
        updateCanTableRealtime({
          timestamp: data.timestamp || Date.now(),
          canId: data.canId,
          data: data.data
        });
        break;
        
      case 'location':
        // Atualiza mapa se houver dados de localização
        if (data.decoded?.coordinates) {
          const [lon, lat] = data.decoded.coordinates;
          // Importa dinamicamente para evitar ciclo
          import('./map.js').then(({ updateMapPosition }) => {
            updateMapPosition(lat, lon);
          });
        }
        break;
        
      default:
        console.log('📨 Tipo de mensagem não tratado:', data.type);
    }
    
  } catch (error) {
    console.error('❌ Erro ao processar mensagem WebSocket:', error, data);
  }
}

/**
 * Estabelece conexão WebSocket com reconexão automática
 */
export function connect() {
  try {
    console.log('🔗 Conectando ao WebSocket:', WS.getUrl());
    
    _ws = new WebSocket(WS.getUrl());

    // 🔹 Evento: conexão estabelecida
    _ws.onopen = () => {
      console.log('✅ WebSocket conectado');
      setServerStatus(true);
      _reconnectAttempts = 0; // Reset contador de reconexões
      
      // Envia mensagem de handshake (opcional)
      _ws.send('🔌 Dashboard conectado');
    };

    // 🔹 Evento: mensagem recebida
    _ws.onmessage = (event) => {
      // Tenta parsear JSON, fallback para texto
      let data;
      try {
        data = JSON.parse(event.data);
      } catch {
        data = event.data;
      }
      handleMessage(data);
    };

    // 🔹 Evento: conexão fechada
    _ws.onclose = (event) => {
      console.log('🔌 WebSocket desconectado', event.code, event.reason);
      setServerStatus(false);
      
      // Reconexão automática com backoff exponencial
      if (_reconnectAttempts < MAX_RECONNECT_ATTEMPTS) {
        const delay = WS.RECONNECT_DELAY * Math.pow(1.5, _reconnectAttempts);
        _reconnectAttempts++;
        console.log(`🔄 Tentando reconectar em ${Math.round(delay/1000)}s... (${_reconnectAttempts}/${MAX_RECONNECT_ATTEMPTS})`);
        setTimeout(connect, delay);
      } else {
        console.error('❌ Máximo de tentativas de reconexão atingido');
      }
    };

    // 🔹 Evento: erro na conexão
    _ws.onerror = (error) => {
      console.error('❌ Erro no WebSocket:', error);
      setServerStatus(false);
    };
    
  } catch (error) {
    console.error('❌ Erro ao criar conexão WebSocket:', error);
    setServerStatus(false);
  }
}

/**
 * Envia mensagem para o servidor via WebSocket
 * @param {Object|string} message - Mensagem a enviar
 * @returns {boolean} True se enviado com sucesso
 */
export function send(message) {
  if (!_ws || _ws.readyState !== WebSocket.OPEN) {
    console.warn('⚠️ WebSocket não está conectado');
    return false;
  }
  
  const payload = typeof message === 'string' ? message : JSON.stringify(message);
  _ws.send(payload);
  return true;
}

/**
 * Fecha a conexão WebSocket manualmente
 */
export function disconnect() {
  if (_ws) {
    _ws.close(1000, 'Desconectado pelo cliente');
    _ws = null;
  }
}

/**
 * Verifica se há conexão ativa
 * @returns {boolean}
 */
export function isConnected() {
  return _ws?.readyState === WebSocket.OPEN;
}

export default {
  connect,
  send,
  disconnect,
  isConnected
};
