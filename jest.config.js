module.exports = {
  // Defina o ambiente de teste
  testEnvironment: 'node',
  // Especifique os padrões de nome para os arquivos de teste
  testMatch: ['**/__tests__/**/*.js?(x)', '**/?(*.)+(spec|test).js?(x)'],
  setupFilesAfterEnv: ["<rootDir>/tests/setup.js"]
};
