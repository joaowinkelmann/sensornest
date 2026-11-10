<p align="center">
  <img src="./web/public/logo.webp" alt="SensorNest logo" width="180" />
</p>

<h1 align="center">sensornest</h1>

<p align="center">
  <a href="#english">English</a> · <a href="#português">Português</a>
</p>

<p align="center">
  <a href="https://github.com/joaowinkelmann/esp-sensornest">esp-sensornest</a> ·
  <a href="https://github.com/joaowinkelmann/api-sensornest">api-sensornest</a> ·
  <a href="https://github.com/joaowinkelmann/front-sensornest">front-sensornest</a>
</p>

---

## English

Unified monorepo for **SensorNest** — TCC (undergraduate thesis) by [João Vitor Winkelmann](https://github.com/joaowinkelmann): an efficient, scalable and secure wireless mesh sensor network platform for environmental monitoring, with encrypted data collection, cloud persistence, and a spatial front-end with controlled user access.

History of the three original repos is preserved via `git subtree` merges (343 commits). Originals stay up as-is:

| Dir | From | Role |
|-----|------|------|
| [`firmware/`](./firmware/) | [esp-sensornest](https://github.com/joaowinkelmann/esp-sensornest) | [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/stable/) firmware: sensor nodes + controller, mesh networking, Wi-Fi provisioning portal |
| [`api/`](./api/) | [api-sensornest](https://github.com/joaowinkelmann/api-sensornest) | [NestJS](https://nestjs.com/) backend: auth, persistence, statistics, map tiling, uploads |
| [`web/`](./web/) | [front-sensornest](https://github.com/joaowinkelmann/front-sensornest) | [Vue 3](https://vuejs.org/) + [Quasar](https://quasar.dev/) admin: spatial visualization, map editor, reports |

```
sensor nodes (ESP32) --ESP-NOW / Wi-Fi Mesh--> controller (ESP32) --HTTPS--> api/ --Postgres/OCI--> web/
```

### Objectives

- Study WSNs, mesh topologies, low-power protocols ([ESP32-C3](https://www.espressif.com/en/products/socs/esp32-c3)), data encryption, user auth, persistence and spatial visualization.
- Build node + controller hardware/firmware with Mesh comms, low power draw and encrypted collection/transmission.
- Build backend ([`api/`](./api/)) for auth, user/device management and cloud persistence, plus front-end ([`web/`](./web/)) for interactive map visualization with access control.
- Validate efficiency, scalability and security: energy tests, mesh range, encrypted-transmission reliability, visualization + access control.

### [`firmware/`](./firmware/)

[ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/stable/) (C). Targets include ESP32, ESP32-C3, ESP32-S3 (energy stress test ran on ESP32-S3 @ 80 MHz).

- [`main/src/app/sensornest.c`](./firmware/main/src/app/sensornest.c) — entry point; [`roles/`](./firmware/main/src/roles/) — `controller_role`, `worker_role`, `role_manager`
- [`net/`](./firmware/main/src/net/) — `espnow_manager`, `wifi_manager`, `rest_client`, `rest_server`, `captive_dns`
- [`sensor/`](./firmware/main/src/sensor/) — sensor readout ([DHT](https://github.com/UncleRus/esp-idf-lib) via `esp-idf-lib/dht`); [`storage/`](./firmware/main/src/storage/) ([LittleFS](https://github.com/joltwallet/esp_littlefs)), [`security/`](./firmware/main/src/security/), [`indicator/`](./firmware/main/src/indicator/) (LED strip), [`front/`](./firmware/front/) — Wi-Fi provisioning portal
- Provisioning flow: first boot with no saved credentials starts open AP `ESP-{shortID}` with captive portal (`index.html`); user connects by phone/PC, enters nearby Wi-Fi credentials and logs into the API; device registers to that user via POST, then leaves setup and works as collector.

```bash
cd firmware
idf.py set-target esp32s3   # or esp32c3
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

Energy notes (from TCC research): [AMS1117](https://www.advanced-monolithic.com/pdf/ds1117.pdf) regulators waste ~4–5 mA quiescent — avoid for battery nodes; prefer efficient SOT-23-5 LDOs and power switching.

### [`api/`](./api/)

[NestJS](https://nestjs.com/) 11 + [Fastify](https://fastify.dev/) + [Prisma](https://www.prisma.io/) 7 + [Postgres](https://www.postgresql.org/). Global prefix `sensornest/api/`, [Swagger](https://swagger.io/) at `sensornest/api/docs`. JWT auth enforced globally (`AuthGuard`), mail via [OCI Email Delivery](https://docs.oracle.com/en-us/iaas/Content/Email/Concepts/overview.htm) (Handlebars templates).

Modules ([`api/src/`](./api/src/)): `auth`, `users`, `meshes`, `zones`, `devices`, `readings`, `statistics` (per-mesh/zone/device metrics: highs, averages over day/week/month/year), `data-exporter`, `uploader` (multipart, 5 MB max), `tiler` (map tiles), `device-checker` (flags inactive devices), `health`, `mailer`.

Data model ([`api/prisma/schema.prisma`](./api/prisma/schema.prisma)): `User`, `Mesh`, `Zone`, `Device`, `Reading` (+ migrations, [`seed.ts`](./api/prisma/seed.ts)).

```bash
cd api
cp .env.template .env   # fill DATABASE_URL, JWT_SECRET, OCI_* (storage bucket sensornest-maps, email, map tiling)
npm install
npx prisma migrate dev
npm run start:dev
```

Key env ([`api/.env.template`](./api/.env.template)): `DATABASE_URL`, `APP_PORT`, `APP_BASE_URL`, `JWT_SECRET`, `JWT_EXPIRES_IN`, `OCI_REGION/TENANCY/USER/FINGERPRINT/PRIVATE_KEY(_PATH)/NAMESPACE/BUCKET`, `OCI_EMAIL_*`, `MAP_TILE_FORMAT/QUALITY/EFFORT/MAX_ZOOM`, `TIME_BEFORE_DEVICE_INACTIVE`.

### [`web/`](./web/)

[Vue 3](https://vuejs.org/) + [Quasar](https://quasar.dev/) 2 + [Vite](https://vite.dev/) 7 + [Pinia](https://pinia.vuejs.org/) + [vue-router](https://router.vuejs.org/) + [axios](https://axios-http.com/). Maps: [Leaflet](https://leafletjs.com/) + [leaflet-draw](https://github.com/Leaflet/Leaflet.draw) (zone editor, 3+ vertices to save). Charts: [ECharts](https://echarts.apache.org/). i18n: PT/EN.

Routes ([`web/src/views/`](./web/src/views/)): `auth/` (login, register, reset-password), `home/`, `meshes/`, `zones/`, `devices/`, `data/`, `map-editor/`, `reports/`.

```bash
cd web
cp .env.template .env   # VITE_API_URL=https://<host>/sensornest/api/ VITE_PORT VITE_OCI_MAP_TOKEN
npm install
npm run dev     # preview: npm run preview | serve: npm start (pm2, port 7772)
npm test        # vitest
```

### History

Merged with `git subtree add --prefix=<dir> <source> main` per component, so `git log --all --graph` shows every original commit. Path-filtered log (`git log -- web/...`) shows one commit per prefix — expected for subtree merges; use `git log --merges` + second parents for archaeology.

### Academic context

Full TCC notes (monograph PDF, energy analyses, provisioning flow sketches, API docs draft) live in the private Notion page `[n] SensorNest (TCC)`. Public portfolio surface is this repo.

---

## Português

Monorepo unificado do **SensorNest** — TCC de [João Vitor Winkelmann](https://github.com/joaowinkelmann): uma plataforma eficiente, escalável e segura de redes de sensores em malha para monitoramento de ambientes, com coleta criptografada, persistência em nuvem e front-end espacial com acesso controlado por usuários.

O histórico dos três repositórios originais foi preservado via merges com `git subtree` (343 commits). Os originais continuam no ar como estão:

| Dir | Origem | Papel |
|-----|--------|-------|
| [`firmware/`](./firmware/) | [esp-sensornest](https://github.com/joaowinkelmann/esp-sensornest) | Firmware [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/stable/): nós sensores + controlador, rede em malha, portal de provisionamento Wi-Fi |
| [`api/`](./api/) | [api-sensornest](https://github.com/joaowinkelmann/api-sensornest) | Backend [NestJS](https://nestjs.com/): autenticação, persistência, estatísticas, tiles de mapa, uploads |
| [`web/`](./web/) | [front-sensornest](https://github.com/joaowinkelmann/front-sensornest) | Admin [Vue 3](https://vuejs.org/) + [Quasar](https://quasar.dev/): visualização espacial, editor de mapa, relatórios |

```
nós sensores (ESP32) --ESP-NOW / Wi-Fi Mesh--> controlador (ESP32) --HTTPS--> api/ --Postgres/OCI--> web/
```

### Objetivos

- Estudar RSSFs, topologias em malha, protocolos de baixo consumo ([ESP32-C3](https://www.espressif.com/en/products/socs/esp32-c3)), criptografia, autenticação, persistência e visualização espacial.
- Construir hardware/firmware dos nós e do controlador com comunicação Mesh, baixo consumo e coleta/transmissão criptografada.
- Construir o backend ([`api/`](./api/)) para autenticação, gestão de usuários/dispositivos e persistência em nuvem, e o front-end ([`web/`](./web/)) para visualização interativa em mapa com controle de acesso.
- Validar eficiência, escalabilidade e segurança: testes de consumo, alcance da malha, confiabilidade da transmissão criptografada, visualização + controle de acesso.

### [`firmware/`](./firmware/)

[ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/stable/) (C). Alvos incluem ESP32, ESP32-C3, ESP32-S3 (teste de estresse energético rodou no ESP32-S3 @ 80 MHz).

- [`main/src/app/sensornest.c`](./firmware/main/src/app/sensornest.c) — ponto de entrada; [`roles/`](./firmware/main/src/roles/) — `controller_role`, `worker_role`, `role_manager`
- [`net/`](./firmware/main/src/net/) — `espnow_manager`, `wifi_manager`, `rest_client`, `rest_server`, `captive_dns`
- [`sensor/`](./firmware/main/src/sensor/) — leitura de sensores ([DHT](https://github.com/UncleRus/esp-idf-lib) via `esp-idf-lib/dht`); [`storage/`](./firmware/main/src/storage/) ([LittleFS](https://github.com/joltwallet/esp_littlefs)), [`security/`](./firmware/main/src/security/), [`indicator/`](./firmware/main/src/indicator/) (fita de LED), [`front/`](./firmware/front/) — portal de provisionamento Wi-Fi
- Fluxo de provisionamento: no primeiro boot sem credenciais salvas, abre um AP aberto `ESP-{shortID}` com portal captive (`index.html`); o usuário conecta pelo celular/PC, informa o Wi-Fi próximo e faz login na API; o dispositivo é registrado para esse usuário via POST e sai do setup para atuar como coletor.

```bash
cd firmware
idf.py set-target esp32s3   # ou esp32c3
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

Notas de energia (pesquisa do TCC): reguladores [AMS1117](https://www.advanced-monolithic.com/pdf/ds1117.pdf) desperdiçam ~4–5 mA em repouso — evitar em nós a bateria; preferir LDOs SOT-23-5 eficientes e chaveamento de alimentação.

### [`api/`](./api/)

[NestJS](https://nestjs.com/) 11 + [Fastify](https://fastify.dev/) + [Prisma](https://www.prisma.io/) 7 + [Postgres](https://www.postgresql.org/). Prefixo global `sensornest/api/`, [Swagger](https://swagger.io/) em `sensornest/api/docs`. Auth JWT global (`AuthGuard`), e-mail via [OCI Email Delivery](https://docs.oracle.com/en-us/iaas/Content/Email/Concepts/overview.htm) (templates Handlebars).

Módulos ([`api/src/`](./api/src/)): `auth`, `users`, `meshes`, `zones`, `devices`, `readings`, `statistics` (métricas por mesh/zona/dispositivo: máximas e médias do dia/semana/mês/ano), `data-exporter`, `uploader` (multipart, máx. 5 MB), `tiler` (tiles de mapa), `device-checker` (marca dispositivos inativos), `health`, `mailer`.

Modelo de dados ([`api/prisma/schema.prisma`](./api/prisma/schema.prisma)): `User`, `Mesh`, `Zone`, `Device`, `Reading` (+ migrations, [`seed.ts`](./api/prisma/seed.ts)).

```bash
cd api
cp .env.template .env   # preencher DATABASE_URL, JWT_SECRET, OCI_* (bucket sensornest-maps, e-mail, tiling)
npm install
npx prisma migrate dev
npm run start:dev
```

Env principal ([`api/.env.template`](./api/.env.template)): `DATABASE_URL`, `APP_PORT`, `APP_BASE_URL`, `JWT_SECRET`, `JWT_EXPIRES_IN`, `OCI_REGION/TENANCY/USER/FINGERPRINT/PRIVATE_KEY(_PATH)/NAMESPACE/BUCKET`, `OCI_EMAIL_*`, `MAP_TILE_FORMAT/QUALITY/EFFORT/MAX_ZOOM`, `TIME_BEFORE_DEVICE_INACTIVE`.

### [`web/`](./web/)

[Vue 3](https://vuejs.org/) + [Quasar](https://quasar.dev/) 2 + [Vite](https://vite.dev/) 7 + [Pinia](https://pinia.vuejs.org/) + [vue-router](https://router.vuejs.org/) + [axios](https://axios-http.com/). Mapas: [Leaflet](https://leafletjs.com/) + [leaflet-draw](https://github.com/Leaflet/Leaflet.draw) (editor de zonas, mín. 3 vértices para salvar). Gráficos: [ECharts](https://echarts.apache.org/). i18n: PT/EN.

Rotas ([`web/src/views/`](./web/src/views/)): `auth/` (login, cadastro, redefinir senha), `home/`, `meshes/`, `zones/`, `devices/`, `data/`, `map-editor/`, `reports/`.

```bash
cd web
cp .env.template .env   # VITE_API_URL=https://<host>/sensornest/api/ VITE_PORT VITE_OCI_MAP_TOKEN
npm install
npm run dev     # preview: npm run preview | servir: npm start (pm2, porta 7772)
npm test        # vitest
```

### Histórico

Unido com `git subtree add --prefix=<dir> <origem> main` por componente; `git log --all --graph` mostra todos os commits originais. Log filtrado por caminho (`git log -- web/...`) mostra um commit por prefixo — esperado em merges subtree; use `git log --merges` + segundos pais para arqueologia.

### Contexto acadêmico

As notas completas do TCC (PDF da monografia, análises energéticas, rascunhos do fluxo de provisionamento, rascunho da doc da API) estão na página privada do Notion `[n] SensorNest (TCC)`. A vitrine pública do portfólio é este repo.
