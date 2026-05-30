/**
 * @fileoverview Controller para gerenciamento de dados de veículo e frames CAN.
 * 
 * Este módulo contém funções para:
 * - Salvar dados do veículo.
 * - Buscar dados históricos e recentes.
 * - Filtrar por deviceId.
 * - Adicionar frames CAN a documentos existentes.
 * - Exportar dados em formatos CSV e JSON.
 * 
 * @module VehicleController
 * @author Alexsandro J Silva
 * @version 1.0.0
 * @since 2025-11-21
 */

const VehicleData = require('../models/canDataModels');
const CanFrame = require('../models/canFrameModels');
const CurrentLocation = require('../models/currentLocationModels'); // mesmo arquivo, modelo diferente
const { decodeCanFrame } = require('../utils/canDecoder');
const { formatTimestamp } = require('../public/js/utils');

/**
 * Salva um novo registro de dados do veículo.
 * 
 * @async
 * @function createVehicleData
 * @param {object} req - Objeto de requisição Express.
 * @param {object} req.body - Dados do veículo a serem salvos.
 * @param {object} res - Objeto de resposta Express.
 * @returns {Promise<void>}
 * @throws {Error} Se ocorrer erro ao salvar no banco de dados.
 * 
 * @example
 * POST /api/device
 * Body: {
 *   "deviceId": "voltz-20250121-143022",
 *   "speed": 45,
 *   "battery": { "soc": 85, "voltage": 350.5 },
 *   "canMessages": [...]
 * }
 */
exports.createVehicleData = async (req, res) => {
  try {
    const data = new VehicleData();
    const savedData = await data.save();
    res.status(201).json(savedData);
  } catch (error) {
    console.error('Erro ao salvar dados do veículo:', error);
    res.status(400).json({
      error: 'Falha ao salvar dados',
      message: error.message
    });
  }
};

/**
 * Retorna todos os registros de um dispositivo específico.
 * 
 * @async
 * @function getVehicleDataByDeviceId
 * @param {object} req - Objeto de requisição Express.
 * @param {object} req.params - Parâmetros da URL.
 * @param {string} req.params.deviceId - ID do dispositivo.
 * @param {object} res - Objeto de resposta Express.
 * @returns {Promise<void>}
 * @throws {Error} Se ocorrer erro ao buscar no banco de dados.
 * 
 * @example
 * GET /api/device/voltz-20250121-143022
 * Response: [{ "_id": "...", "timestamp": "...", ... }]
 */
exports.getVehicleData = async (req, res) => {
  const limit = parseInt(req.query.limit) || 20;
  //const { deviceId } = req.params;

  try {
    const data = await VehicleData
      .find({})
      .sort({ timestamp: -1 })
      .lean() // reduzir o uso de memória
      .limit(limit)
      .exec();

    if (data.length === 0) {
      return res.status(404).json({ error: `Nenhum dado encontrado` });
    }


    res.json(data);
  } catch (error) {
    console.error(`Erro ao buscar dados`, error);
    res.status(500).json({ error: 'Erro interno do servidor' });
  }
};

/**
 * Adiciona um ou vários frames CAN diretamente na coleção CanFrame
 * 
 * @async
 * @function addCanMessage
 * @param {object} req - Objeto de requisição Express.
 * @param {object} req.params - Parâmetros da URL.
 * @param {string} req.params.deviceId - ID do dispositivo.
 * @param {object|object[]} req.body - Frame CAN ou array de frames.
 * @param {object} res - Objeto de resposta Express.
 * 
 * @example
 * POST /api/can/voltz-20250121-143022
 * Body: [
 *   { "canId": 288, "data": [166, 121, 24, 236], "dlc": 4, "rtr": false },
 *   { "canId": 768, "data": [25, 28, 54, 48], "dlc": 4, "rtr": false }
 * ]
 */
exports.addCanMessage = async (req, res) => {
  try {
    //const { deviceId } = req.params;
    const canMessages = Array.isArray(req.body) ? req.body : [req.body];

    // Validação básica
    for (const msg of canMessages) {
      if (!msg || !msg.canId || !msg.data) {
        return res.status(400).json({
          error: 'Dados incompletos',
          message: 'Cada frame CAN deve conter canId e data'
        });

        
      }
      
      const hexArray = msg.data.split(' '); // Transforma em ["09", "D8", ...]
      const numericData = hexArray.map(hex => parseInt(hex, 16)); // Converte para [9, 216, 14, ...]
      msg.data = numericData; // Adiciona o array numérico para facilitar consultas futuras
    }

    
    const framesToInsert = canMessages.map(msg => ({

      canId: msg.canId,
      data: msg.data,
      dlc: msg.dlc,
      ide: msg.ide || false,
      timestamp: msg.ts
    }));

    

    framesToInsert.forEach(element => {
      const decodedFrame = decodeCanFrame(element);

      if (decodedFrame) {

        processDecodedFrame(decodedFrame, element.timestamp);
      }

    });
    
    // Insere todos os frames de uma só vez
    const result = await CanFrame.insertMany(framesToInsert);

    res.status(201).json({
      message: `Adicionados ${result.length} frames com sucesso`,
      insertedCount: result.length
    });
  } catch (error) {
    console.error('Erro ao adicionar mensagens CAN:', error);
    res.status(500).json({
      success: false,
      error: 'Falha ao adicionar mensagens CAN',
      message: error.message
    });
  }
};

/**
 * Retorna os últimos N frames CAN (padrão: 20).
 * 
 * @async
 * @function getRecentCanData
 * @param {object} req - Objeto de requisição Express.
 * @param {object} req.query - Parâmetros da query string.
 * @param {number} [req.query.limit=20] - Número máximo de frames a retornar.
 * @param {object} res - Objeto de resposta Express.
 * @returns {Promise<void>}
 * @throws {Error} Se ocorrer erro ao buscar no banco de dados.
 * 
 * @example
 * GET /api/can-data?limit=20
 * Response: [
 *   { "canId": 288, "data": [166, 121, 24, 236], "timestamp": "..." },
 *   ...
 * ]
 */
exports.getRecentCanData = async (req, res) => {
  // Define o limite com trava de segurança
  const limit = Math.min(parseInt(req.query.limit) || 50, 1000);

  try {
    // 1. Defina a query (vazia {} busca todos os registros)
    const query = {};

    // 2. Busca no banco
    const frames = await CanFrame
      .find(query)             // Agora a variável existe
      .sort({ timestamp: -1 }) // Garante que os mais novos venham primeiro
      .limit(limit)
      .lean();                 // Melhora performance (retorna objeto puro JS)

    res.json(frames);
  } catch (error) {
    console.error('Erro ao buscar frames:', error);
    res.status(500).json({ error: 'Erro interno ao buscar dados CAN' });
  }
};

/**
 * Exporta todos os dados CAN do banco como CSV (com streaming para evitar memory leak).
 * 
 * @async
 * @function exportAllCanDataAsCsv
 * @param {object} req - Objeto de requisição Express.
 * @param {string} [req.query.deviceId] - Filtrar por deviceId específico (opcional).
 * @param {object} res - Objeto de resposta Express.
 * @returns {Promise<void>}
 * @throws {Error} Se ocorrer erro ao buscar ou formatar os dados.
 * 
 * @example
 * GET /api/export-can-data-csv
 * Download: can-data-1234567890.csv
 */
exports.exportAllCanDataAsCsv = async (req, res) => {
  const { deviceId } = req.query;
  const query = deviceId ? { deviceId } : {};

  res.setHeader('Content-Type', 'text/csv');
  res.setHeader('Content-Disposition', `attachment; filename=can-data-${Date.now()}.csv`);
  res.write('timestamp,canId,data,dlc,rtr\n');

  // Usa streaming com cursor
  const cursor = CanFrame.find(query).sort({ timestamp: 1 }).cursor();
  for await (const frame of cursor) {
    const row = [
      `"${formatTimestamp(frame.timestamp)}"`,
      `"0x${frame.canId.toString(16).toUpperCase()}"`,
      `"${frame.data.join(' ')}"`,
      frame.dlc,
      frame.rtr ? 'true' : 'false'
    ];
    res.write(row.join(',') + '\n');
  }
  res.end();
};

/**
 * Exporta todos os dados de VehicleData como CSV (com streaming)
 */
exports.exportVehicleDataAsCsv = async (req, res) => {
  const { deviceId } = req.query;
  const query = deviceId ? { deviceId } : {};

  // Define cabeçalho do CSV
  const headers = [
    'timestamp',
    'deviceId',
    'battery.soc',
    'battery.soh',
    'battery.voltage',
    'battery.current',
    'battery.temperature',
    'motor.modo',
    'motor.rpm',
    'motor.torque',
    'motor.motorTemp',
    'motor.controlTemp',
    'accuracy',
    'altitude',
    'altitudeAccuracy',
    'heading',
    'speed',
    'location.coordinates'
  ];

  // Configura resposta CSV
  res.setHeader('Content-Type', 'text/csv');
  res.setHeader('Content-Disposition', `attachment; filename=vehicle-data-${Date.now()}.csv`);
  res.write(headers.join(',') + '\n');

  // Usa cursor para streaming eficiente
  const cursor = VehicleData
    .find(query)
    .sort({ timestamp: 1 })
    .cursor();

  for await (const doc of cursor) {
    const row = [
      formatTimestamp(doc.timestamp) || '',
      doc.deviceId || '',
      doc.battery?.soc ?? '',
      doc.battery?.soh ?? '',
      doc.battery?.voltage ?? '',
      doc.battery?.current ?? '',
      doc.battery?.temperature ?? '',
      doc.motor?.modo ?? '',
      doc.motor?.rpm ?? '',
      doc.motor?.torque ?? '',
      doc.motor?.motorTemp ?? '',
      doc.motor?.controlTemp ?? '',
      doc.speed ?? '',
      doc.altitude ?? '',
      doc.altitudeAccuracy ?? '',
      doc.heading ?? '',
      doc.accuracy ?? '',
      doc.location?.coordinates ? `"${doc.location.coordinates.join(',')}"` : ''
    ].map(field => {
      // Escapa aspas dentro dos campos (boa prática)
      if (typeof field === 'string') {
        return field.replace(/"/g, '""');
      }
      return field;
    });

    res.write(row.join(',') + '\n');
  }

  res.end();
};

/**
 * Processa um frame decodificado e salva como novo registro histórico,
 * mesclando com a localização GPS mais recente.
 * 
 * @param {Object} decodedFrame - Dados decodificados do CAN/frame
 * @param {Date|Number} timestamp - Timestamp do evento
 */
async function processDecodedFrame(decodedFrame, timestamp) {
  try {
    // ========================================
    // 1️⃣ BUSCA DADOS AUXILIARES EM PARALELO
    // ========================================

    // Busca a localização GPS atual (modelo singleton)
    const currentGPS = await CurrentLocation.findOne()
      .sort({ createdAt: -1 })
      .select('location speed altitude altitudeAccuracy heading accuracy')
      .lean();
    // Busca o último registro histórico completo
    const lastRecord = await VehicleData.findOne()
      .sort({ timestamp: -1 })
      .lean();


    // ========================================
    // 2️⃣ PREPARA BASE DE DADOS
    // ========================================

    // Base: último registro OU objeto vazio
    const base = lastRecord || {};

    // ========================================
    // 3️⃣ FUNÇÕES AUXILIARES DE MERGE
    // ========================================

    /**
     * Retorna o valor do frame, ou fallback para base, ou undefined
     * Ignora valores "zerados" indesejados (opcional, ajuste conforme necessidade)
     */
    const getValue = (frameKey, baseKey = frameKey, ignoreZero = false) => {
      const frameVal = decodedFrame[frameKey];
      const baseVal = base[baseKey];

      if (frameVal !== undefined && frameVal !== null) {
        // Se ignorar zero: só usa se for diferente de 0
        if (ignoreZero && frameVal === 0) return baseVal;
        return frameVal;
      }
      return baseVal;
    };

    /**
     * Mescla subdocumentos (ex: battery, motor) de forma segura
     */
    const mergeSubdoc = (key) => ({
      ...(base[key] || {}),
      ...(decodedFrame[key] || {})
    });

    // ========================================
    // 4️⃣ MONTA NOVO REGISTRO
    // ========================================
    
    const newRecord = {
      // 🔹 Mescla simples: decodedFrame → currentGPS → base → null
      speed: currentGPS?.speed ?? null ?? undefined,
      altitude:  currentGPS?.altitude ?? null ?? undefined,
      altitudeAccuracy:  currentGPS?.altitudeAccuracy ??  null ?? undefined,
      heading: currentGPS?.heading ?? null ?? undefined,
      accuracy: currentGPS?.accuracy ?? null ?? undefined,


      // 🔹 Subdocumentos: merge profundo
      battery: mergeSubdoc('battery'),
      motor: mergeSubdoc('motor'),

      // 🔹 Localização: prioridade máxima para GPS atual
      // Se decodedFrame trouxer location válida, usa ela
      // Senão, tenta usar a do CurrentLocation
      // Por último, mantém a do histórico (se existir)
      location: (() => {
        // 1. Frame tem coordenadas válidas?
        if (decodedFrame.location?.coordinates?.length === 2) {
          return {
            type: 'Point',
            coordinates: decodedFrame.location.coordinates
          };
        }
        // 2. CurrentLocation tem dados válidos?
        if (currentGPS?.location?.coordinates?.length === 2) {
          return {
            type: 'Point',
            coordinates: currentGPS.location.coordinates
          };
        }
        // 3. Fallback: mantém do histórico
        return base.location || undefined;
      })(),

      // 🔹 Campos de controle do Mongoose (forçar novos)
      _id: undefined,
      createdAt: undefined,
      updatedAt: undefined,

      // 🔹 Timestamp do evento (obrigatório)
      timestamp: timestamp
    };

    // ========================================
    // 5️⃣ FILTRA CAMPOS UNDEFINED (OPCIONAL)
    // ========================================

    // Remove chaves com valor undefined para não sobrescrever com null no banco
    const cleanRecord = Object.fromEntries(
      Object.entries(newRecord).filter(([_, v]) => v !== undefined)
    );

    // ========================================
    // 6️⃣ SALVA NOVO DOCUMENTO
    // ========================================

    const doc = new VehicleData(cleanRecord);
    await doc.save();
    return doc; // Retorna para chaining ou testes

  } catch (error) {
    console.error('❌ Erro ao processar frame:', {
      message: error.message,
      stack: error.stack,
      frameKeys: decodedFrame ? Object.keys(decodedFrame) : 'N/A'
    });

    // Lança o erro para quem chamou poder tratar (opcional)
    throw error;
  }
}
