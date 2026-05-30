// models/CanFrame.js
const mongoose = require('mongoose');

const canFrameSchema = new mongoose.Schema({
  deviceId: { type: String, required: false, index: true },
  canId: { type: Number, required: true },
  data: [{ type: Number, required: true }],
  dlc: { type: Number, required: true },
  ide: { type: Boolean, default: false },
  timestamp: { type: Date, index: true }
});

// Índices para consultas rápidas
canFrameSchema.index({ deviceId: 1, timestamp: -1 });

module.exports = mongoose.model('CanFrame', canFrameSchema);