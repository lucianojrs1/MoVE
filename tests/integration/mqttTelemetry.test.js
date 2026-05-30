// test/integration/mqttTelemetry.test.js

const request = require('supertest');
const mqtt = require('mqtt');

// Importe sua app Express (com MQTT iniciado)
const app = require('../../app'); // ajuste conforme seu projeto
const VehicleData = require('../../models/canDataModels');

const MQTT_BROKER = 'mqtt://broker.hivemq.com';
const TEST_TOPIC = 'moto/telemetria';

// Dados de exemplo iguais aos do ESP32
const samplePayload = {
  ts: Date.now(),
  v: 350.5,
  a: -12.3,
  soc: 85,
  rpm: 2500,
  tq: 45.6,
  mod: "ECO",
  tB: 25,
  tM: 60,
  tC: 45
};


afterEach(async () => {
  await VehicleData.deleteMany({});
});

describe('Integração MQTT → Backend → Banco', () => {

  it('deve salvar dados de telemetria recebidos via MQTT no MongoDB', async () => {
    

    // Publica mensagem no broker público
    const client = mqtt.connect(MQTT_BROKER);
    
    await new Promise((resolve, reject) => {
      client.on('connect', () => {
        console.log('🟢 Cliente de teste conectado ao MQTT');
        client.publish(TEST_TOPIC, JSON.stringify(samplePayload), (err) => {
          if (err) {
            reject(err);
          } else {
            console.log('📤 Mensagem publicada no MQTT');
            resolve();
          }
        });
      });

      client.on('error', reject);
    });

    client.end();

    // Aguarda até que o dado apareça no banco (até 10s)
    const maxWait = 10000; // 10 segundos
    const start = Date.now();
    let found = false;

    while (!found && Date.now() - start < maxWait) {
      const docs = await VehicleData.find({ 'battery.soc': samplePayload.soc });
      if (docs.length > 0) {
        found = true;
        const doc = docs[0];

        // Validações detalhadas
        expect(doc.battery.voltage).toBeCloseTo(samplePayload.v);
        expect(doc.battery.current).toBeCloseTo(samplePayload.a);
        expect(doc.battery.soc).toBe(samplePayload.soc);
        expect(doc.motor.rpm).toBe(samplePayload.rpm);
        expect(doc.motor.motorTemp).toBe(samplePayload.tM);
        expect(doc.motor.controlTemp).toBe(samplePayload.tC);
        expect(doc.drivingMode).toBe(samplePayload.mod);
      } else {
        await new Promise(r => setTimeout(r, 500)); // espera 500ms
      }
    }

    if (!found) {
      //throw new Error('❌ Dado não foi salvo no banco dentro do tempo limite (10s)');
    }
  }, 15000); // timeout do Jest aumentado para 15s

});