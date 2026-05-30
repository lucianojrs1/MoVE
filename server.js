
process.env.NODE_TLS_REJECT_UNAUTHORIZED = '0'; // Ela desativa a verificação de certificados para todas as requisições feitas pelo seu processo Node

const { app } = require('./app');
const fs = require('fs');
const { connectDB } = require('./database/db');
const { handleWebSocketMessage, addData, sendMessage } = require('./utils/handleWebSocketMessage');
const https = require('https');
const WebSocket = require('ws');
const PORT = process.env.PORT || 3001;
const path = require('path');

const { connectMQTT } = require('./mqtt/mqttClient');

const sslOptions = {
  key: fs.readFileSync(path.join(__dirname, 'config', 'key.pem.example')),
  cert: fs.readFileSync(path.join(__dirname, 'config', 'cert.pem.example'))
};

const server = https.createServer(sslOptions, app);

const wss = new WebSocket.Server({ server });

wss.on('connection', async (ws, req) => {

  ws.on('message', (message) => {
    handleWebSocketMessage(wss, ws, message, req );
  });

  ws.on('close', () => {
    if (ws.deviceId) {
      console.log(`❌🔌 Esp Desconectado : ${ws.deviceId}`);
      sendMessage(wss, ws,`❌🔌 ESP32 Desconectado ${ws.deviceId}`);
    } else {
      console.log('❌🔌 Dashboard Desconectado');
    }
  });

  ws.on('error', (error) => {
    if (ws.deviceId) {
      console.log(`❌🔌 Erro no Esp : ${ws.deviceId}`);
      sendMessage(wss, ws,`❌🔌 ESP32 Desconectado ${ws.deviceId}`);
    } else {
      console.log('❌🔌 Erro no Dashboard');
    }
    console.error('❌ Erro no WebSocket:', error);    
  });

  ws.send(JSON.stringify({ message: 'Conectado ao servidor WebSocket' }));
});


connectDB()
  .then(() => {
    server.listen(PORT, async () => {
      console.log(`🟢 Servidor rodando na porta ${PORT}`);
      //console.log(`WebSocket disponível em ws://localhost:${PORT}`);
    });
  })
  .catch((err) => {
    console.error('❌ Erro ao iniciar o servidor:', err);
  });

  // Iniciar cliente MQTT
connectMQTT();
