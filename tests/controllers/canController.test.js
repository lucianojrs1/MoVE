const request = require('supertest');
const mongoose = require('mongoose');
const { app } = require('../../app'); // ou onde estiver seu app Express
// Models
const VehicleData = require('../../models/canDataModels');
const CanFrame = require('../../models/canFrameModels');

// Mock do console.error para evitar poluir o terminal
jest.spyOn(console, 'error').mockImplementation(() => { });

// Mock do decoder (opcional)
jest.mock('../../utils/canDecoder', () => ({
  decodeCanFrame: jest.fn().mockImplementation((frame) => {
    if (frame.canId === 288) {
      return { type: 'speed', data: { speed: 45 } };
    }
    return null;
  })
}));

// Dados de exemplo
const mockVehicleData = {
  deviceId: 'voltz-20250121-143022',
  speed: 45,
  battery: {
    soc: 85,
    voltage: 350.5
  },
  canMessages: [
    {
      canId: 288,
      data: [166, 121, 24, 236],
      dlc: 4,
      rtr: false
    }
  ]
};
const deviceId = 'voltz-test-device';
const validFrame = {
  canId: 288,
  data: [166, 121, 24, 236],
  dlc: 4,
  rtr: false
};

describe('Vehicle Controller - API Tests', () => {
  // Limpa a coleção antes de cada teste
  beforeEach(async () => {
    await CanFrame.deleteMany({});
  });

  afterAll(async () => {
    await VehicleData.deleteMany({});
    await CanFrame.deleteMany({});
  });
  

   describe('POST /api/device', () => {
    it('should create a new vehicle data record', async () => {
      const payload = {
        deviceId: 'test-device-001',
        speed: 60,
        battery: { soc: 90, voltage: 360.2 }
      };

      const res = await request(app)
        .post('/api/device')
        .send(payload)
        .expect(201);

      expect(res.body.deviceId).toBe('test-device-001');
      expect(res.body.speed).toBe(60);
    });
    
  });
  
  describe('POST /api/can/:deviceId', () => {
    it('should add a single CAN message', async () => {
      const res = await request(app)
        .post('/api/can/test-device-003')
        .send({
          canId: 288,
          data: [166, 121, 24, 236],
          dlc: 4
        })
        .expect(201);

      expect(res.body.message).toContain('Adicionados 1 frames com sucesso');
    });

    it('should add multiple CAN messages', async () => {
      const messages = [
        { canId: 288, data: [1, 2, 3, 4], dlc: 4 },
        { canId: 768, data: [5, 6, 7, 8], dlc: 4 }
      ];

      const res = await request(app)
        .post('/api/can/test-device-004')
        .send(messages)
        .expect(201);

      expect(res.body.insertedCount).toBe(2);
    });

    it('should return 400 if message is missing canId or data', async () => {
      const res = await request(app)
        .post('/api/can/test-device-005')
        .send({ dlc: 4 }) // faltando canId e data
        .expect(400);

      expect(res.body.error).toBe('Dados incompletos');
    });
  });

  describe('GET /api/can-data', () => {
    it('should return recent CAN frames (default limit 50)', async () => {
      await CanFrame.insertMany([
        { deviceId: 'd1', canId: 288, data: [1, 2], dlc: 2 },
        { deviceId: 'd1', canId: 768, data: [3, 4], dlc: 2 }
      ]);

      const res = await request(app)
        .get('/api/can-data')
        .expect(200);

      expect(res.body).toHaveLength(2);
      expect(res.body[0].canId).toBeDefined();
    });

    it('should filter by deviceId when provided', async () => {
      await CanFrame.insertMany([
        { deviceId: 'target', canId: 100, data: [1], dlc: 1 },
        { deviceId: 'other', canId: 200, data: [2], dlc: 1 }
      ]);

      const res = await request(app)
        .get('/api/can-data?deviceId=target')
        .expect(200);

      expect(res.body).toHaveLength(1);
      expect(res.body[0].deviceId).toBe('target');
    });
  });

  describe('POST /api/can/:deviceId', () => {
    it('deve adicionar um único frame CAN com sucesso', async () => {
      const response = await request(app)
        .post(`/api/can/${deviceId}`)
        .send(validFrame)
        .expect(201);

      expect(response.body).toHaveProperty('message');
      expect(response.body.insertedCount).toBe(1);

      // Verifica se o frame foi salvo no banco
      const savedFrames = await CanFrame.find({ deviceId });
      expect(savedFrames).toHaveLength(1);
      expect(savedFrames[0].canId).toBe(validFrame.canId);
      expect(savedFrames[0].data).toEqual(validFrame.data);
    });

    it('deve adicionar múltiplos frames CAN com sucesso', async () => {
      const frames = [
        { ...validFrame, canId: 288 },
        { ...validFrame, canId: 768 },
        { ...validFrame, canId: 512 }
      ];

      const response = await request(app)
        .post(`/api/can/${deviceId}`)
        .send(frames)
        .expect(201);

      expect(response.body.insertedCount).toBe(3);

      // Verifica se todos os frames foram salvos
      const savedFrames = await CanFrame.find({ deviceId });
      expect(savedFrames).toHaveLength(3);
      expect(savedFrames.map(f => f.canId)).toEqual([288, 768, 512]);
    });

    it('deve retornar 400 se o body estiver vazio', async () => {
      const response = await request(app)
        .post(`/api/can/${deviceId}`)
        .send({})
        .expect(400);

      expect(response.body.error).toBe('Dados incompletos');
    });

    it('deve retornar 400 se faltar canId em um frame', async () => {
      const invalidFrame = { ...validFrame };
      delete invalidFrame.canId;

      const response = await request(app)
        .post(`/api/can/${deviceId}`)
        .send(invalidFrame)
        .expect(400);

      expect(response.body.error).toBe('Dados incompletos');
    });

    it('deve retornar 400 se faltar data em um frame', async () => {
      const invalidFrame = { ...validFrame };
      delete invalidFrame.data;

      const response = await request(app)
        .post(`/api/can/${deviceId}`)
        .send(invalidFrame)
        .expect(400);

      expect(response.body.error).toBe('Dados incompletos');
    });

    it('deve retornar 400 se um frame em um array for inválido', async () => {
      const frames = [
        { ...validFrame, canId: 288 },
        { canId: 768 } // Faltando data
      ];

      const response = await request(app)
        .post(`/api/can/${deviceId}`)
        .send(frames)
        .expect(400);

      expect(response.body.error).toBe('Dados incompletos');
    });


    it('deve gerar timestamp automaticamente', async () => {
      await request(app)
        .post(`/api/can/${deviceId}`)
        .send(validFrame)
        .expect(201);

      const savedFrame = await CanFrame.findOne({ deviceId });
      expect(savedFrame.timestamp).toBeInstanceOf(Date);
    });

    it('deve retornar 500 se ocorrer erro no banco de dados', async () => {
      // Simula erro no banco de dados
      jest.spyOn(CanFrame, 'insertMany').mockImplementationOnce(() => {
        throw new Error('Database error');
      });

      const response = await request(app)
        .post(`/api/can/${deviceId}`)
        .send(validFrame)
        .expect(500);

      expect(response.body.error).toBe('Falha ao adicionar mensagens CAN');
    });
  });
  
  describe('GET /api/decoded-can-data', () => {
    it('should return decoded CAN frames with decoding info', async () => {
      await CanFrame.create({
        deviceId: 'decode-test',
        canId: 288,
        data: [166, 121, 24, 236],
        dlc: 4
      });

      const res = await request(app)
        .get('/api/decoded-can-data?limit=1')
        .expect(200);

      expect(res.body).toHaveLength(1);
      expect(res.body[0].decoded).not.toBeNull();
      expect(res.body[0].source).toBe('speed');
    });
  });
  /*
  describe('GET /api/export-can-data-csv', () => {
    it('should return CSV content with correct headers and rows', async () => {
      await CanFrame.create({
        deviceId: 'csv-test',
        canId: 0x1200,
        data: [10, 20, 30],
        dlc: 3,
        rtr: false
      });

      const res = await request(app)
        .get('/api/export-can-data-csv')
        .expect(200);

      expect(res.headers['content-type']).toContain('text/csv');
      expect(res.headers['content-disposition']).toMatch(/^attachment; filename=can-data-\d+\.csv$/);
      expect(res.text).toContain('timestamp,canId,data,dlc,rtr');
      expect(res.text).toContain('0x120');
      expect(res.text).toContain('10 20 30');
    });

    it('should filter by deviceId in CSV export', async () => {
      await CanFrame.insertMany([
        { deviceId: 'A', canId: 1, data: [1], dlc: 1 },
        { deviceId: 'B', canId: 2, data: [2], dlc: 1 }
      ]);

      const res = await request(app)
        .get('/api/export-can-data-csv?deviceId=A')
        .expect(200);

      const lines = res.text.split('\n').filter(l => l.trim());
      // Cabeçalho + 1 linha de dado
      expect(lines).toHaveLength(2);
      expect(res.text).toContain('0x1');
      expect(res.text).not.toContain('0x2');
    });
  });
  */
});