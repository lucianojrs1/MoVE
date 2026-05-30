// models/VehicleData.js
const mongoose = require('mongoose');

const vehicleDataSchema = new mongoose.Schema({
  // Identificador do dispositivo (moto)
  deviceId: {
    type: String,
    required: true,
    trim: true,

    default: () => {
      const now = new Date();
      const year = now.getFullYear();
      const month = String(now.getMonth() + 1).padStart(2, '0');
      const day = String(now.getDate()).padStart(2, '0');
      const hour = String(now.getHours()).padStart(2, '0');
      const minute = String(now.getMinutes()).padStart(2, '0');
      const second = String(now.getSeconds()).padStart(2, '0');
      const formatted = `${year}${month}${day}-${hour}${minute}${second}`;
      return `voltz-${formatted}`;
    }

  },

  // Timestamp principal do evento
  timestamp: {
    type: Date,
  },
  // === DADOS INTERPRETADOS (para dashboard) ===

  // Bateria
  battery: {
    soc: { type: Number }, // State of Charge (%)
    soh: { type: Number }, // State of Health (%)
    voltage: { type: Number }, // Volts
    current: { type: Number }, // Amperes (negativo = carga)
    temperature: { type: Number } // °C
  },

  // Motor
  motor: {
    rpm: { type: Number },
    torque: { type: Number }, // Nm
    motorTemp: { type: Number },
    controlTemp: { type: Number },
    modo: { type: String }
  },

  location: {
    type: { type: String, enum: ['Point'] },
    coordinates: { type: [Number] } // aceita qualquer array (ou ausente)
  },

  // Precisão do GPS (opcional, fora de location)
  accuracy: {
    type: Number,
  },
  speed: {
    type: Number,
  },
  altitude: {
    type: Number,
  },
  altitudeAccuracy: {
    type: Number,
  },
  heading: {
    type: Number,
  }


}, {
  timestamps: true, // createdAt, updatedAt

});


module.exports = mongoose.model('VehicleData', vehicleDataSchema);