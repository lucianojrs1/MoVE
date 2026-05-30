// controllers/currentLocationController.js
const CurrentLocation = require('../models/currentLocationModels'); // Ajuste o caminho conforme sua estrutura
const VehicleData = require('../models/canDataModels');

/**
 * @desc    Buscar a localização atual (último registro)
 * @route   GET /api/current-location
 * @access  Public
 */
const getCurrentLocation = async (req, res) => {
  try {
    // Busca o documento mais recente ordenando por createdAt decrescente
    // Como só deve existir um, pegamos o primeiro resultado
    const location = await CurrentLocation.findOne()
      .sort({ createdAt: -1 }) // -1 = decrescente (mais recente primeiro)
      .lean(); // .lean() retorna um POJO simples, mais performático para leitura

    // Se não encontrar nenhum registro, retorna 404
    if (!location) {
      return res.status(404).json({
        success: false,
        message: 'Nenhuma localização registrada ainda.'
      });
    }

    // Retorna os dados com sucesso
    return res.status(200).json({
      success: true,
      data: location
    });

  } catch (error) {
    console.error('Erro ao buscar localização:', error);
    return res.status(500).json({
      success: false,
      message: 'Erro interno no servidor.',
      error: error.message
    });
  }
};

/**
 * @desc    Criar ou Atualizar a localização atual (UPSERT)
 * @route   POST /api/current-location ou PUT /api/current-location
 * @access  Public
 */
const upsertCurrentLocation = async (req, res) => {
  try {
    // Extrai os dados do body da requisição
    const {
      location,
      accuracy,
      speed,
      altitude,
      altitudeAccuracy,
      heading
    } = req.body;



    // Validação básica: location.coordinates é obrigatório para ser útil
    if (!location || !location.coordinates || !Array.isArray(location.coordinates)) {
      return res.status(400).json({
        success: false,
        message: 'Campo "location.coordinates" é obrigatório e deve ser um array [longitude, latitude].'
      });
    }

    // Valida se coordinates tem pelo menos 2 elementos [longitude, latitude]
    if (location.coordinates.length < 2) {
      return res.status(400).json({
        success: false,
        message: 'Coordinates deve conter pelo menos [longitude, latitude].'
      });
    }

    // Prepara o objeto de atualização com os campos permitidos
    const updateData = {
      location: {
        type: 'Point', // Força o tipo GeoJSON padrão
        coordinates: location.coordinates
      },
      accuracy,
      speed,
      altitude,
      altitudeAccuracy,
      heading
      // timestamps: true no schema cuida de createdAt/updatedAt automaticamente
    };

    // findOneAndUpdate com upsert: true
    // - Filtra: documento vazio {} pois só deve existir um
    // - Update: dados novos
    // - Options: 
    //   * new: true -> retorna o documento atualizado (não o antigo)
    //   * upsert: true -> cria se não existir
    //   * setDefaultsOnInsert: true -> aplica defaults do schema se criar novo
    const updatedLocation = await CurrentLocation.findOneAndUpdate(
      {}, // filtro vazio: atua sobre qualquer documento (só existe um)
      { $set: updateData },
      {
        new: true,
        upsert: true,
        setDefaultsOnInsert: true,
        runValidators: true // garante que validações do schema sejam aplicadas
      }
    ).lean();

    // Determina o status HTTP baseado se foi criado ou atualizado
    // isNew não está disponível com lean(), então verificamos por createdAt === updatedAt (simplificação)
    const wasCreated = updatedLocation.createdAt?.getTime() === updatedLocation.updatedAt?.getTime();
    const statusCode = wasCreated ? 201 : 200;
    
    
    return res.status(statusCode).json({
      success: true,
      message: wasCreated ? 'Localização criada com sucesso.' : 'Localização atualizada com sucesso.',
      data: updatedLocation
    });

  } catch (error) {
    console.error('Erro ao salvar localização:', error);

    // Tratamento específico para erros de validação do Mongoose
    if (error.name === 'ValidationError') {
      return res.status(400).json({
        success: false,
        message: 'Erro de validação.',
        errors: Object.values(error.errors).map(err => err.message)
      });
    }

    return res.status(500).json({
      success: false,
      message: 'Erro interno no servidor.',
      error: error.message
    });
  }
};

/**
 * @desc    Deletar a localização atual (opcional - use com cautela)
 * @route   DELETE /api/current-location
 * @access  Public
 */
const deleteCurrentLocation = async (req, res) => {
  try {
    // DeleteOne com filtro vazio: remove o primeiro documento encontrado
    // Como só deve existir um, isso é seguro no seu contexto
    const result = await CurrentLocation.deleteOne({});

    // Se nenhum documento foi removido
    if (result.deletedCount === 0) {
      return res.status(404).json({
        success: false,
        message: 'Nenhuma localização encontrada para deletar.'
      });
    }

    return res.status(200).json({
      success: true,
      message: 'Localização deletada com sucesso.'
    });

  } catch (error) {
    console.error('Erro ao deletar localização:', error);
    return res.status(500).json({
      success: false,
      message: 'Erro interno no servidor.',
      error: error.message
    });
  }
};

// Exporta todas as funções do controller
module.exports = {
  getCurrentLocation,
  upsertCurrentLocation,
  deleteCurrentLocation
};