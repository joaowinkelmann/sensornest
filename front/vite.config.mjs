import { fileURLToPath, URL } from "node:url";
import { quasar, transformAssetUrls } from "@quasar/vite-plugin";
import vue from "@vitejs/plugin-vue";
import AutoImport from "unplugin-auto-import/vite";
import Components from "unplugin-vue-components/vite";
import { QuasarResolver } from "unplugin-vue-components/resolvers";
import { VueRouterAutoImports } from "unplugin-vue-router";
import VueRouter from "unplugin-vue-router/vite";
import Layouts from "vite-plugin-vue-layouts-next";
import { defineConfig } from "vite";

// Switch to function form to detect build vs dev
export default defineConfig(({ command }) => {
  const isBuild = command === "build";

  return {
    plugins: [
      VueRouter({
        dts: isBuild ? false : "src/typed-router.d.ts",
        routesFolder: "src/pages",
        extensions: [".vue"],
      }),
      Layouts(),
      vue({
        template: { transformAssetUrls },
      }),

      AutoImport({
        imports: [
          "vue",
          VueRouterAutoImports,
          { pinia: ["defineStore", "storeToRefs"] },
        ],
        // Avoid mkdir('') on Windows during build
        eslintrc: {
          enabled: !isBuild,
          filepath: "./.eslintrc-auto-import.json",
        },
        vueTemplate: true,
        dts: "src/auto-imports.d.ts",
      }),

      Components({
        resolvers: [QuasarResolver()],
        dts: "src/components.d.ts",
      }),

      quasar({ sassVariables: "src/quasar-variables.sass" }),
    ],

    build: {
      outDir: "dist",
      cssCodeSplit: false,
      emptyOutDir: true,
    },

    optimizeDeps: {
      exclude: [
        "vue-router",
        "unplugin-vue-router/runtime",
        "unplugin-vue-router/data-loaders",
        "unplugin-vue-router/data-loaders/basic",
      ],
    },

    define: { "process.env": {} },

    resolve: {
      alias: {
        "@": fileURLToPath(new URL("src", import.meta.url)),
      },
      extensions: [".js", ".json", ".jsx", ".mjs", ".ts", ".tsx", ".vue"],
    },

    server: {
      port: 7773,
      proxy: {
        "/api": {
          target: "http://192.168.4.1",
          changeOrigin: true,
          ws: true,
          configure: (proxy) => {
            proxy.on("proxyReq", (proxyReq, req) => {
              proxyReq.removeHeader("cookie");
              proxyReq.removeHeader("user-agent");
              proxyReq.removeHeader("accept-encoding");
              proxyReq.removeHeader("accept-language");
              proxyReq.removeHeader("cache-control");
              proxyReq.removeHeader("pragma");
              proxyReq.removeHeader("referer");
              proxyReq.removeHeader("origin");
              proxyReq.removeHeader("sec-fetch-dest");
              proxyReq.removeHeader("sec-fetch-mode");
              proxyReq.removeHeader("sec-fetch-site");
              proxyReq.removeHeader("sec-fetch-user");
              proxyReq.removeHeader("sec-ch-ua");
              proxyReq.removeHeader("sec-ch-ua-mobile");
              proxyReq.removeHeader("sec-ch-ua-platform");
              proxyReq.removeHeader("sec-ch-ua-full-version");
              proxyReq.removeHeader("sec-ch-ua-full-version-list");
              proxyReq.removeHeader("upgrade-insecure-requests");
              proxyReq.removeHeader("x-requested-with");
              proxyReq.removeHeader("dnt");
              proxyReq.removeHeader("te");

              proxyReq.setHeader("host", "dashboard.local");
              proxyReq.setHeader("accept", "application/json");

              if (req.method === "POST" && req.headers["content-type"]) {
                proxyReq.setHeader("content-type", req.headers["content-type"]);
              }

              const finalHeaders = proxyReq.getHeaders();
              const headerSize = Object.entries(finalHeaders).reduce(
                (size, [key, value]) => size + key.length + value.length + 4,
                0,
              );
              console.log(
                `ESP32 Request: ${req.method} ${req.url} (${
                  Object.keys(finalHeaders).length
                } headers, ~${headerSize} bytes)`,
              );
            });
          },
        },
      },
    },

    css: {
      preprocessorOptions: {
        sass: { api: "modern-compiler" },
        scss: { api: "modern-compiler" },
      },
    },
  };
});
