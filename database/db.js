const mongoose = require('mongoose');
const { MongoMemoryServer } = require('mongodb-memory-server');

// Carrega variáveis de ambiente
require('dotenv').config();
process.env.MONGOMS_DEBUG = "1";
let mongoServer;

const connectDB = async () => {
  try {
    // Verifica se estamos usando o banco em memória
    const useInMemoryDB = process.env.DEV;

    if (useInMemoryDB === 'true') {
      // Inicia o MongoDB em memória
      mongoServer = await MongoMemoryServer.create();
      const uri = mongoServer.getUri();
      await mongoose.connect(uri);
      console.log('Conectado ao MongoDB em memória.');
    } else {
      // Conecta ao MongoDB cloud
      //const uri = `mongodb+srv://${process.env.DB_USER}:${process.env.DB_PASS}@${process.env.DB_HOST}/?retryWrites=true&w=majority&appName=${process.env.DB_NAME}`;

      const uri = `mongodb://${process.env.DB_USER}:${process.env.DB_PASS}@${process.env.DB_HOST}:${process.env.DB_PORT}/${process.env.DB_NAME}?authSource=admin`;
      await mongoose.connect(uri);
      console.log('Conectado ao MongoDB.');
    }
  } catch (error) {
    console.error('Erro ao conectar ao MongoDB:', error);
    process.exit(1); // Encerra o processo em caso de erro
  }
};

const disconnectDB = async () => {
  try {
    await mongoose.disconnect();
    if (mongoServer) {
      await mongoServer.stop();
      console.log('MongoDB em memória desconectado.');
      // Limpa todas as coleções após cada teste

      const collections = mongoose.connection.collections;

      for (const key in collections) {
        const collection = collections[key];
        await collection.deleteMany({});
      }

    } else {
      console.log('MongoDB real desconectado.');
    }

  } catch (error) {
    console.error('Erro ao desconectar do MongoDB:', error);
  }
};

module.exports = { connectDB, disconnectDB };