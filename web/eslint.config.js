import { defineConfig, globalIgnores } from 'eslint/config'
import globals from 'globals'
import js from '@eslint/js'
import pluginVue from 'eslint-plugin-vue'
import skipFormatting from '@vue/eslint-config-prettier/skip-formatting'

export default defineConfig([
  {
    name: 'app/files-to-lint',
    files: ['**/*.{js,mjs,jsx,vue}'],
  },

  {
    name: 'config/node-globals',
    files: ['vite.config.js', 'vitest.config.js', 'eslint.config.js', 'jsconfig.json'],
    languageOptions: {
      globals: {
        ...globals.node,
      },
    },
  },

  globalIgnores(['**/dist/**', '**/dist-ssr/**', '**/coverage/**', 'jsconfig.json']),

  {
    name: 'vue/rules',
    files: ['**/*.vue'],
    languageOptions: {
      globals: {
        ...globals.vue,
      },
    },
  },

  {
    languageOptions: {
      globals: {
        ...globals.browser,
      },
    },
  },

  js.configs.recommended,
  ...pluginVue.configs['flat/essential'],
  skipFormatting,
])
