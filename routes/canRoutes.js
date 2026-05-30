// routes/vehicleRoutes.js
const express = require('express');
const router = express.Router();

const {
  createVehicleData,
  getVehicleData,
  addCanMessage,
  getRecentCanData,
  exportAllCanDataAsCsv,
  exportVehicleDataAsCsv,
} = require('../controllers/canController');

const {
  upsertCurrentLocation,
  getCurrentLocation
} = require('../controllers/currentLocationController');

// ============================================================================
// 📋 TAGS
// ============================================================================
/**
 * @swagger
 * tags:
 *   - name: Vehicle Data
 *     description: "Telemetria do veículo (bateria, motor, GPS)"
 *   - name: CAN Bus
 *     description: "Frames CAN brutos e exportação de dados"
 *   - name: Location
 *     description: "Localização em tempo real (GeoJSON Point)"
 */

// ============================================================================
// 🗃️ SCHEMAS - Componentes OpenAPI 3.0
// ============================================================================

/**
 * @swagger
 * components:
 *   schemas:
 *     VehicleData:
 *       type: object
 *       properties:
 *         _id:
 *           type: string
 *           description: "ObjectId do MongoDB"
 *           example: "64f8a1b2c3d4e5f6a7b8c9d0"
 *         deviceId:
 *           type: string
 *           description: "ID do dispositivo (formato: voltz-YYYYMMDD-HHMMSS)"
 *           example: "voltz-20240115-143022"
 *         timestamp:
 *           type: string
 *           format: date-time
 *           description: "Timestamp do evento de telemetria"
 *           example: "2024-01-15T14:30:22.000Z"
 *         battery:
 *           type: object
 *           properties:
 *             soc:
 *               type: number
 *               minimum: 0
 *               maximum: 100
 *               description: "State of Charge (%)"
 *               example: 85.5
 *             soh:
 *               type: number
 *               minimum: 0
 *               maximum: 100
 *               description: "State of Health (%)"
 *               example: 98.2
 *             voltage:
 *               type: number
 *               format: float
 *               description: "Tensão em Volts"
 *               example: 58.4
 *             current:
 *               type: number
 *               format: float
 *               description: "Corrente em Amperes (negativo = carga)"
 *               example: -12.3
 *             temperature:
 *               type: number
 *               format: float
 *               description: "Temperatura da bateria em °C"
 *               example: 32.5
 *         motor:
 *           type: object
 *           properties:
 *             rpm:
 *               type: integer
 *               minimum: 0
 *               description: "Rotações por minuto"
 *               example: 4500
 *             torque:
 *               type: number
 *               format: float
 *               description: "Torque em Newton-metros"
 *               example: 45.8
 *             motorTemp:
 *               type: number
 *               format: float
 *               description: "Temperatura do motor em °C"
 *               example: 55.0
 *             controlTemp:
 *               type: number
 *               format: float
 *               description: "Temperatura do controlador em °C"
 *               example: 48.2
 *             modo:
 *               type: string
 *               description: "Modo de operação do motor"
 *               example: "eco"
 *               enum: ["eco", "normal", "sport", "regen"]
 *         location:
 *           type: object
 *           properties:
 *             type:
 *               type: string
 *               enum: ["Point"]
 *               example: "Point"
 *             coordinates:
 *               type: array
 *               items:
 *                 type: number
 *               minItems: 2
 *               maxItems: 2
 *               description: "[longitude, latitude] - ordem GeoJSON"
 *               example: [-46.6333, -23.5505]
 *         accuracy:
 *           type: number
 *           format: float
 *           description: "Precisão do GPS em metros"
 *           example: 5.2
 *         speed:
 *           type: number
 *           format: float
 *           description: "Velocidade em m/s"
 *           example: 12.5
 *         altitude:
 *           type: number
 *           format: float
 *           description: "Altitude em metros"
 *           example: 760.5
 *         altitudeAccuracy:
 *           type: number
 *           format: float
 *           description: "Precisão da altitude em metros"
 *           example: 10.0
 *         heading:
 *           type: number
 *           format: float
 *           minimum: 0
 *           maximum: 360
 *           description: "Direção em graus (0° = Norte)"
 *           example: 180.5
 *         createdAt:
 *           type: string
 *           format: date-time
 *           readOnly: true
 *         updatedAt:
 *           type: string
 *           format: date-time
 *           readOnly: true
 *
 *     CanFrame:
 *       type: object
 *       required:
 *         - canId
 *         - data
 *         - dlc
 *       properties:
 *         _id:
 *           type: string
 *           description: "ObjectId do MongoDB"
 *         deviceId:
 *           type: string
 *           description: "ID do dispositivo associado (opcional)"
 *           example: "voltz-20240115-143022"
 *         canId:
 *           type: integer
 *           description: "ID do frame CAN em decimal (ex: 0x18FEF100 = 419373312)"
 *           example: 419373312
 *         data:
 *           type: array
 *           items:
 *             type: integer
 *             minimum: 0
 *             maximum: 255
 *           minItems: 0
 *           maxItems: 8
 *           description: "Payload do frame (0-8 bytes)"
 *           example: [16, 20, 114, 0, 0, 0, 0, 0]
 *         dlc:
 *           type: integer
 *           minimum: 0
 *           maximum: 8
 *           description: "Data Length Code (número de bytes válidos em 'data')"
 *           example: 8
 *         ide:
 *           type: boolean
 *           description: "Identifier Extension (true = frame estendido 29-bit)"
 *           default: false
 *         timestamp:
 *           type: string
 *           format: date-time
 *           description: "Timestamp de recepção do frame"
 *           example: "2024-01-15T14:30:22.123Z"
 *
 *     CurrentLocation:
 *       type: object
 *       properties:
 *         _id:
 *           type: string
 *           description: "ObjectId do MongoDB"
 *           example: "64f8a1b2c3d4e5f6a7b8c9d0"
 *         location:
 *           type: object
 *           properties:
 *             type:
 *               type: string
 *               enum: ["Point"]
 *               description: "Tipo de geometria GeoJSON"
 *               example: "Point"
 *             coordinates:
 *               type: array
 *               items:
 *                 type: number
 *               minItems: 2
 *               maxItems: 2
 *               description: "[longitude, latitude] - padrão GeoJSON"
 *               example: [-46.6333, -23.5505]
 *         accuracy:
 *           type: number
 *           format: float
 *           minimum: 0
 *           description: "Precisão horizontal do GPS em metros"
 *           example: 5.2
 *         speed:
 *           type: number
 *           format: float
 *           minimum: 0
 *           description: "Velocidade instantânea em m/s"
 *           example: 12.5
 *         altitude:
 *           type: number
 *           format: float
 *           description: "Altitude em metros acima do nível do mar"
 *           example: 760.5
 *         altitudeAccuracy:
 *           type: number
 *           format: float
 *           minimum: 0
 *           description: "Precisão da medida de altitude em metros"
 *           example: 10.0
 *         heading:
 *           type: number
 *           format: float
 *           minimum: 0
 *           maximum: 360
 *           description: "Direção de movimento em graus (0° = Norte)"
 *           example: 180.5
 *         createdAt:
 *           type: string
 *           format: date-time
 *           readOnly: true
 *         updatedAt:
 *           type: string
 *           format: date-time
 *           readOnly: true
 */

// ============================================================================
// 🚗 ENDPOINTS - Vehicle Data
// ============================================================================

/**
 * @swagger
 * /device:
 *   post:
 *     summary: "Cria novo registro de telemetria do veículo"
 *     description: "Registra dados de bateria, motor e GPS enviados pelo ESP32. O campo deviceId é gerado automaticamente se não for enviado."
 *     tags: ["Vehicle Data"]
 *     requestBody:
 *       required: true
 *       content:
 *         application/json:
 *           schema:
 *             $ref: "#/components/schemas/VehicleData"
 *           examples:
 *             telemetria-completa:
 *               summary: "Registro completo de telemetria"
 *               value:
 *                 deviceId: "voltz-20240115-143022"
 *                 timestamp: "2024-01-15T14:30:22.000Z"
 *                 battery:
 *                   soc: 85.5
 *                   soh: 98.2
 *                   voltage: 58.4
 *                   current: -12.3
 *                   temperature: 32.5
 *                 motor:
 *                   rpm: 4500
 *                   torque: 45.8
 *                   motorTemp: 55.0
 *                   controlTemp: 48.2
 *                   modo: "eco"
 *                 location:
 *                   type: "Point"
 *                   coordinates: [-46.6333, -23.5505]
 *                 accuracy: 5.2
 *                 speed: 12.5
 *                 altitude: 760.5
 *                 heading: 180.5
 *     responses:
 *       201:
 *         description: "Registro criado com sucesso"
 *         content:
 *           application/json:
 *             schema:
 *               $ref: "#/components/schemas/VehicleData"
 *       400:
 *         description: "Dados inválidos"
 *         content:
 *           application/json:
 *             schema:
 *               $ref: "#/components/schemas/ErrorResponse"
 *       500:
 *         description: "Erro interno do servidor"
 */
router.post('/device', createVehicleData);

/**
 * @swagger
 * /device/{deviceId}:
 *   get:
 *     summary: "Busca registros de telemetria por deviceId"
 *     description: "Retorna histórico de dados para um dispositivo específico"
 *     tags: ["Vehicle Data"]
 *     parameters:
 *       - in: path
 *         name: deviceId
 *         required: true
 *         schema:
 *           type: string
 *         description: "ID do dispositivo (ex: voltz-20240115-143022)"
 *         example: "voltz-20240115-143022"
 *       - in: query
 *         name: startDate
 *         schema:
 *           type: string
 *           format: date-time
 *         description: "Filtrar registros a partir desta data"
 *       - in: query
 *         name: endDate
 *         schema:
 *           type: string
 *           format: date-time
 *         description: "Filtrar registros até esta data"
 *       - in: query
 *         name: limit
 *         schema:
 *           type: integer
 *           default: 100
 *           maximum: 1000
 *         description: "Limite de registros retornados"
 *     responses:
 *       200:
 *         description: "Lista de registros encontrados"
 *         content:
 *           application/json:
 *             schema:
 *               type: array
 *               items:
 *                 $ref: "#/components/schemas/VehicleData"
 *       404:
 *         description: "Nenhum registro encontrado"
 *         content:
 *           application/json:
 *             schema:
 *               $ref: "#/components/schemas/ErrorResponse"
 */
router.get('/device', getVehicleData);

// ============================================================================
// 📡 ENDPOINTS - CAN Bus
// ============================================================================

/**
 * @swagger
 * /can/{deviceId}:
 *   post:
 *     summary: "Adiciona frame CAN a um dispositivo"
 *     description: "Anexa um frame bruto da rede CAN bus ao histórico do veículo."
 *     tags: ["CAN Bus"]
 *     parameters:
 *       - in: path
 *         name: deviceId
 *         required: true
 *         schema:
 *           type: string
 *         description: "ID do dispositivo alvo"
 *         example: "voltz-20240115-143022"
 *     requestBody:
 *       required: true
 *       content:
 *         application/json:
 *           schema:
 *             $ref: "#/components/schemas/CanFrame"
 *           examples:
 *             frame-obd2-pid:
 *               summary: "Frame OBD-II com PID de RPM"
 *               value:
 *                 canId: 419373312
 *                 data: [16, 20, 114, 0, 0, 0, 0, 0]
 *                 dlc: 8
 *                 ide: true
 *     responses:
 *       201:
 *         description: "Frame registrado com sucesso"
 *         content:
 *           application/json:
 *             schema:
 *               type: object
 *               properties:
 *                 message:
 *                   type: string
 *                   example: "Frame CAN adicionado"
 *                 frame:
 *                   $ref: "#/components/schemas/CanFrame"
 *       400:
 *         description: "Dados inválidos ou incompletos"
 *         content:
 *           application/json:
 *             schema:
 *               $ref: "#/components/schemas/ErrorResponse"
 *       404:
 *         description: "deviceId não encontrado"
 */
router.post('/can', addCanMessage);

/**
 * @swagger
 * /can-data:
 *   get:
 *     summary: "Lista frames CAN recentes"
 *     description: "Retorna os últimos frames recebidos, com filtros opcionais"
 *     tags: ["CAN Bus"]
 *     parameters:
 *       - in: query
 *         name: limit
 *         schema:
 *           type: integer
 *           default: 50
 *           minimum: 1
 *           maximum: 500
 *         description: "Número máximo de frames a retornar"
 *       - in: query
 *         name: deviceId
 *         schema:
 *           type: string
 *         description: "Filtrar por deviceId específico"
 *       - in: query
 *         name: canId
 *         schema:
 *           type: integer
 *         description: "Filtrar por ID CAN específico (em decimal)"
 *         example: 419373312
 *     responses:
 *       200:
 *         description: "Lista de frames CAN"
 *         content:
 *           application/json:
 *             schema:
 *               type: array
 *               items:
 *                 $ref: "#/components/schemas/CanFrame"
 */
router.get('/can-data', getRecentCanData);

// ============================================================================
// 📥 ENDPOINTS - Exportação CSV
// ============================================================================

/**
 * @swagger
 * /export-can-data-csv:
 *   get:
 *     summary: "Exporta frames CAN como CSV"
 *     description: "Gera arquivo CSV com histórico de frames para análise externa"
 *     tags: ["CAN Bus"]
 *     parameters:
 *       - in: query
 *         name: deviceId
 *         schema:
 *           type: string
 *         description: "Filtrar exportação por deviceId"
 *       - in: query
 *         name: startDate
 *         schema:
 *           type: string
 *           format: date
 *         description: "Data inicial (YYYY-MM-DD)"
 *       - in: query
 *         name: endDate
 *         schema:
 *           type: string
 *           format: date
 *         description: "Data final (YYYY-MM-DD)"
 *     responses:
 *       200:
 *         description: "Arquivo CSV gerado"
 *         headers:
 *           Content-Disposition:
 *             schema:
 *               type: string
 *               example: "attachment; filename=\"can-frames-export.csv\""
 *         content:
 *           text/csv:
 *             schema:
 *               $ref: "#/components/schemas/CsvFile"
 */
router.get('/export-can-data-csv', exportAllCanDataAsCsv);

/**
 * @swagger
 * /export-vehicle-data-csv:
 *   get:
 *     summary: "Exporta telemetria do veículo como CSV"
 *     description: "Gera arquivo CSV com dados de bateria, motor e GPS"
 *     tags: ["Vehicle Data"]
 *     parameters:
 *       - in: query
 *         name: deviceId
 *         schema:
 *           type: string
 *         description: "Filtrar por deviceId"
 *       - in: query
 *         name: startDate
 *         schema:
 *           type: string
 *           format: date
 *         description: "Data inicial (YYYY-MM-DD)"
 *       - in: query
 *         name: endDate
 *         schema:
 *           type: string
 *           format: date
 *         description: "Data final (YYYY-MM-DD)"
 *     responses:
 *       200:
 *         description: "Arquivo CSV gerado"
 *         headers:
 *           Content-Disposition:
 *             schema:
 *               type: string
 *               example: "attachment; filename=\"vehicle-telemetry-export.csv\""
 *         content:
 *           text/csv:
 *             schema:
 *               $ref: "#/components/schemas/CsvFile"
 */
router.get('/export-vehicle-data-csv', exportVehicleDataAsCsv);

// ============================================================================
// 📍 ENDPOINTS - Localização (CurrentLocation)
// ============================================================================

/**
 * @swagger
 * /device/location:
 *   post:
 *     summary: "Atualiza ou cria localização atual (upsert)"
 *     description: "Registra as coordenadas GPS mais recentes de um veículo. Formato GeoJSON: coordinates: [longitude, latitude]"
 *     tags: ["Location"]
 *     requestBody:
 *       required: true
 *       content:
 *         application/json:
 *           schema:
 *             $ref: "#/components/schemas/CurrentLocationInput"
 *           examples:
 *             gps-minimo:
 *               summary: "Apenas coordenadas essenciais"
 *               value:
 *                 deviceId: "voltz-20240115-143022"
 *                 location:
 *                   type: "Point"
 *                   coordinates: [-46.6333, -23.5505]
 *             gps-completo:
 *               summary: "Coordenadas com todos os metadados"
 *               value:
 *                 deviceId: "voltz-20240115-143022"
 *                 location:
 *                   type: "Point"
 *                   coordinates: [-46.6333, -23.5505]
 *                 accuracy: 5.2
 *                 speed: 12.5
 *                 altitude: 760.5
 *                 heading: 180.5
 *     responses:
 *       200:
 *         description: "Localização criada ou atualizada com sucesso"
 *         content:
 *           application/json:
 *             schema:
 *               type: object
 *               properties:
 *                 success:
 *                   type: boolean
 *                   example: true
 *                 data:
 *                   $ref: "#/components/schemas/CurrentLocation"
 *                 message:
 *                   type: string
 *                   example: "Localização atualizada com sucesso"
 *       400:
 *         description: "Dados inválidos ou formato GeoJSON incorreto"
 *         content:
 *           application/json:
 *             schema:
 *               $ref: "#/components/schemas/ErrorResponse"
 */
router.post('/device/location', upsertCurrentLocation);

/**
 * @swagger
 * /device/location:
 *   get:
 *     summary: "Obtém a localização mais recente de um dispositivo"
 *     description: "Retorna o último registro de GPS armazenado para o deviceId informado"
 *     tags: ["Location"]
 *     parameters:
 *       - in: query
 *         name: deviceId
 *         required: true
 *         schema:
 *           type: string
 *         description: "ID do dispositivo para consulta"
 *         example: "voltz-20240115-143022"
 *     responses:
 *       200:
 *         description: "Localização encontrada"
 *         content:
 *           application/json:
 *             schema:
 *               type: object
 *               properties:
 *                 success:
 *                   type: boolean
 *                   example: true
 *                 data:
 *                   $ref: "#/components/schemas/CurrentLocation"
 *                 deviceId:
 *                   type: string
 *                   example: "voltz-20240115-143022"
 *       404:
 *         description: "Nenhuma localização registrada para este deviceId"
 *         content:
 *           application/json:
 *             schema:
 *               $ref: "#/components/schemas/ErrorResponse"
 */
router.get('/device/location', getCurrentLocation);

module.exports = router;