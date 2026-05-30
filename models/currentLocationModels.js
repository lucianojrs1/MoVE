// models/VehicleData.js
const mongoose = require('mongoose');

const currentLocationSchema = new mongoose.Schema({
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


module.exports = mongoose.model('CurrentLocation', currentLocationSchema);