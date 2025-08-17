import js from "@eslint/js";
import globals from "globals";
import vuePlugin from "eslint-plugin-vue";
import prettierPlugin from "eslint-plugin-prettier";

export default {
  languageOptions: {
    ecmaVersion: "latest",
    sourceType: "module",
    globals: {
      ...globals.browser,
      ...globals.node,
    },
  },
  plugins: {
    vue: vuePlugin,
    prettier: prettierPlugin,
  },
  rules: {
    ...js.configs.recommended.rules,
    ...vuePlugin.configs["recommended"].rules,
    "prettier/prettier": "error",
    "vue/multi-word-component-names": "off",
  },
  ignores: ["node_modules/**", "dist/**"],
};
