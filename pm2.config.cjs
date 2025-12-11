module.exports = {
  name: "api-sensornest", // Name of your application
  script: "dist/src/main.js",
  interpreter: "bun", // Bun interpreter
  env: {
    PATH: `${process.env.HOME}/.bun/bin:${process.env.PATH}`, // Add "~/.bun/bin/bun" to PATH
    PORT: 7771
  },
};