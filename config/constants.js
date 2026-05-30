// config/constants.js
require('dotenv').config();

module.exports = {
  BASE_BATTERY_ID: parseInt(process.env.BASE_BATTERY_ID, 16),
  BASE_CONTROLLER_ID: parseInt(process.env.BASE_CONTROLLER_ID, 16)
};