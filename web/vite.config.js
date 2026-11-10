import { fileURLToPath, URL } from 'node:url'
import { defineConfig, loadEnv } from 'vite'
import vue from '@vitejs/plugin-vue'
import { quasar, transformAssetUrls } from '@quasar/vite-plugin'

export default defineConfig(({ mode }) => {
  // Load all env variables
  const env = loadEnv(mode, process.cwd(), '')

  // Create a new object to hold only the variables that should be exposed to the client
  const processEnv = {}
  for (const key in env) {
    // Only expose variables that are explicitly prefixed with VITE_
    if (key.startsWith('VITE_')) {
      processEnv[key] = env[key]
    }
  }

  return {
    base: '/sensornest/',
    plugins: [
      vue({
        template: { transformAssetUrls }
      }),
      quasar({
        sassVariables: '/src/css/quasar.variables.sass',
      })
    ],
    resolve: {
      alias: {
        '@': fileURLToPath(new URL('./src', import.meta.url))
      },
    },
    server: {
      port: parseInt(env.VITE_PORT || '3000', 10),
      allowedHosts: ['winkels.com.br'],
    },
    define: {
      // Use the filtered object instead of the full 'env'
      'process.env': processEnv
    }
  }
})