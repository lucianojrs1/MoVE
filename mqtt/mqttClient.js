// mqtt/mqttClient.js
const mqtt = require('mqtt');
const { addCanMessage, getDecodedCanData } = require('../utils/api');

const MQTT_BROKER = process.env.MQTT_BROKER;
const MQTT_TOPIC = process.env.MQTT_TOPIC;
const API_URL = process.env.API_URL;

let client;

async function connectMQTT() {
  client = mqtt.connect(MQTT_BROKER, {
    reconnectPeriod: 3000,
    keepalive: 60
  });

  client.on('connect', () => {
    console.log(`✅ Conectado ao broker MQTT: ${MQTT_BROKER}`);
    client.subscribe(MQTT_TOPIC, (err) => {
      if (err) {
        console.error(`❌ Falha ao subscrever tópico ${MQTT_TOPIC}:`, err);
      } else {
        console.log(`📡 Subscrito ao tópico: ${MQTT_TOPIC}`);
      }
    });
  });

  client.on('message', async (topic, message) => {
    if (topic === MQTT_TOPIC) {
      try {
        const payload = message.toString();
        const data = JSON.parse(payload);
        //data.deviceId = data.deviceId || 'unknown-device'; // Garantir que deviceId exista
        
        await addCanMessage(data); // Enviar para API

        

      } catch (error) {
        console.error('❌ Erro ao processar mensagem MQTT:', error.message, message.toString());
      }
    }
  });

  client.on('error', (err) => {
    console.error('❌ Erro no cliente MQTT:', err.message);
  });

  client.on('reconnect', () => {
    console.log('🔄 Reconectando ao broker MQTT...');
  });

  client.on('close', () => {
    console.log('🔌 Conexão MQTT fechada');
  });
}

module.exports = { connectMQTT };