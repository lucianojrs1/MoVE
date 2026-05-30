/**
 * @fileoverview Módulo para processamento de mensagens WebSocket do ESP32 e persistência de dados CAN no MongoDB.
 * 
 * Este módulo contém funções para:
 * - Adicionar frames CAN diretamente ao banco de dados.
 * - Processar mensagens recebidas via WebSocket.
 * - Decodificar dados CAN e enviar para o frontend.
 * - Gerenciar o ID do dispositivo para persistência.
 * 
 * @module WebSocketHandler
 * @author Alexsandro j Silva
 * @version 1.0.0
 * @since 2027-11-21
 */

const axios = require('axios');
const { decodeCanFrame, } = require('../utils/canDecoder');
const VehicleData = require('../models/canDataModels'); // Importe seu modelo
const CanFrame = require('../models/canFrameModels');

const uri = `${process.env.API_URL}`;
const WebSocket = require('ws');

function sendMessage(wss, ws, message) {
  wss.clients.forEach((client) => {
    if (client.readyState === WebSocket.OPEN && client !== ws) {
      client.send(JSON.stringify(message));
    }
  });
}

// Buffer global para armazenar frames CAN
const canBuffer = [];

// Limite do buffer (envia quando atingir este número)
const BUFFER_LIMIT = 100;

/**
 * Adiciona um frame CAN ao buffer
 */
function addBuffer(canFrame, deviceId) {
  if (!deviceId) {
    console.warn('⚠️ deviceId não definido, ignorando frame CAN');
    return;
  }

  // Adiciona frame ao buffer com seu deviceId
  canBuffer.push({ canFrame, deviceId });

  // Verifica se o buffer atingiu o limite
  if (canBuffer.length >= BUFFER_LIMIT) {
    processBuffer(); // Processa todos os frames do buffer
  }
}


async function processBuffer() {
  const useInMemoryDB = process.env.DEV;

  if (canBuffer.length === 0) return;

  const framesToInsert = canBuffer.map(({ canFrame, deviceId }) => ({
    deviceId,
    ...canFrame,
    timestamp: new Date()
  }));

  try {
    // so inserir em memoria se for o caso de Deploy
    if (useInMemoryDB === 'false') {
      // salva todos os frames de uma vez
      await CanFrame.insertMany(framesToInsert);
    }
    
    canBuffer.length = 0; // ← Só limpa se der sucesso
  } catch (error) {
    console.error('❌ Erro ao inserir frames:', error);
    // canBuffer permanece intacto para nova tentativa
  }
}

/**
 * Adiciona dados iniciais para o dispositivo no banco de dados.
 * 
 * @async
 * @function addData
 * @returns {Promise<string|null>} O deviceId gerado ou null em caso de erro.
 */
async function addData() {
  const mockData = {
    location: {
      type: 'Point',
      coordinates: [-46.5755, -23.6789]
    },
  };

  try {
    const response = await axios.post(`${uri}/api/device`, mockData);
    console.log(`📊 Dados de teste inseridos com sucesso`);
    console.log(`🔑 deviceId:`, response.data.deviceId || response.data.savedData.deviceId);
    deviceId = response.data.deviceId;
    return deviceId;
  } catch (error) {
    console.log(`📊 Erro ao inserir dados:`, error.response?.data?.message || error.message);
    return null;
  }
}

/**
 * Processa mensagens recebidas via WebSocket do ESP32.
 * 
 * @async
 * @function handleWebSocketMessage
 * @param {WebSocket} ws - Conexão WebSocket do cliente (ESP32).
 * @param {Buffer} message - Mensagem recebida do cliente.
 * @param {Set<WebSocket>} allClients - Conjunto de todos os clientes conectados (ex: dashboards).
 * @returns {Promise<void>}
 * Esta função:
 * - Faz parse da mensagem JSON.
 * - Processa frames CAN recebidos.
 * - Decodifica os dados (bateria/motor) e envia para o frontend.
 * - Salva os frames no banco de dados.
 * - Reenvia mensagens para outros clientes conectados.
 */
async function handleWebSocketMessage(wss, ws, message, req) {
  try {

    const raw = message.toString().trim();

    // Primeira mensagem do ESP32: identificação
    if (raw === "ESP32 Conectado ao WebSocket!") {
      addData().then(deviceId => {
        ws.deviceId = deviceId;
        console.log(`🔌 ESP32 Conectado: ${deviceId} | IP: ${req.socket.remoteAddress}`);
        // Notifica outros clientes (dashboards) sobre a nova conexão do ESP32
        sendMessage(wss, ws, `🔌 ESP32 Conectado ${deviceId}`);
      });
      return;
    } else if (raw === "🔌 Dashboard Conectado ao WebSocket!") {
      console.log(`🔌 Dashboard Conectado IP: ${req.socket.remoteAddress}`);
    }

    const rawMessage = message.toString().trim(); // trim uma vez

    if (rawMessage.startsWith('{') && rawMessage.endsWith('}') && ws.deviceId) {
      const data = JSON.parse(rawMessage);

      if (typeof data !== 'object' || data === null) {
        console.error('❌ Dados recebidos inválidos:', rawMessage);
        return;
      }

      // Processa mensagem CAN recebida do ESP32
      if (data.type === "canFrame") {
        const canFrame = {
          canId: data.id,
          data: data.data,
          dlc: data.dlc,
          ide: data.extended || false
        };

        // Adiciona ao buffer em vez de salvar imediatamente
        addBuffer(canFrame, ws.deviceId);

        // Cria o objeto para envio reaproveitando as propriedades anteriores
        const messagePayload = {
          ...canFrame,
          type: "canFrame"
        };
        sendMessage(wss, ws, messagePayload);
        const decoded = decodeCanFrame(canFrame);
        const validTypes = ['battery', 'motorController'];

        if (decoded && validTypes.includes(decoded.type)) {
          const decodedData = {
            type: decoded.type,
            decoded: decoded.data
          };
          sendMessage(wss, ws, decodedData);
        }
      }
    }

  } catch (error) {
    console.error('❌ Erro ao processar mensagem do ESP32:', error);
    console.error('Mensagem recebida:', message.toString());
  }
}

module.exports = {
  handleWebSocketMessage,
  addData,
  sendMessage,
};