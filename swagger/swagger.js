const swaggerJsDoc = require('swagger-jsdoc');
const swaggerUi = require('swagger-ui-express');

// Opções do Swagger
const swaggerOptions = {
  definition: {
    openapi: '3.0.0',
    info: {
      title: 'Voltz - API de Monitoramento CAN',
      version: '1.0.0',
      description: 'API para monitoramento de dados CAN de veículos elétricos',
      contact: {
        name: 'Alexsandro J Silva',
        email: 'ajs6@cin.ufpe.br'
      }
    },
    servers: [
      {
        url: 'http://localhost:3001/api',
        description: 'Servidor de desenvolvimento'
      }
    ],
    components: {
      schemas: {
        VehicleData: {
          type: 'object',
          properties: {
            _id: { type: 'string', example: '65abc123...' },
            deviceId: { type: 'string', example: 'voltz-20250121-143022' },
            timestamp: { type: 'string', format: 'date-time', example: '2025-01-21T17:30:22.123Z' },
            speed: { type: 'number', example: 45 },
            battery: {
              type: 'object',
              properties: {
                soc: { type: 'number', example: 85 },
                voltage: { type: 'number', example: 350.5 }
              }
            },
            canMessages: {
              type: 'array',
              items: {
                $ref: '#/components/schemas/CanMessage'
              }
            }
          }
        },
        CanMessage: {
          type: 'object',
          properties: {
            canId: { type: 'number', example: 288 },
            data: { // ✅ Corrigido: dentro de `properties`
              type: 'array',
              items: { type: 'number' },
              example: [166, 121, 24, 236]
            },
            dlc: { type: 'number', example: 4 },
            rtr: { type: 'boolean', example: false }
          }
        }
      }
    }
  },
  apis: ['./routes/*.js', './controllers/*.js'] // Caminhos onde estão as anotações
};

const swaggerDocs = swaggerJsDoc(swaggerOptions);

module.exports = (app) => {
  app.use('/api-docs', swaggerUi.serve, swaggerUi.setup(swaggerDocs));
};