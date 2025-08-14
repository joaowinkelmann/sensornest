import { fileURLToPath, URL } from 'node:url'
import { defineConfig, loadEnv } from 'vite'
import vue from '@vitejs/plugin-vue'
import vueDevTools from 'vite-plugin-vue-devtools'
import { quasar, transformAssetUrls } from '@quasar/vite-plugin'

export default defineConfig(({ mode }) => {
  // Carrega as variaveis de ambiente do arquivo .env
  const env = loadEnv(mode, process.cwd(), '')
  return {
    base: '/tempmesh/',
    plugins: [
      vue({
        template: { transformAssetUrls }
      }),
      vueDevTools(),
      quasar({
        sassVariables: '/quasar.variables.sass',
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
  }
})