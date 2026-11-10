# sensornest

Unified monorepo for **SensorNest** — TCC (undergraduate thesis) by João Vitor Winkelmann: an efficient, scalable and secure wireless mesh sensor network platform for environmental monitoring, with encrypted data collection, cloud persistence, and a spatial front-end with controlled user access.

History of the three original repos is preserved via `git subtree` merges (342 commits). Originals stay up as-is:

| Dir | From | Role |
|-----|------|------|
| `firmware/` | [esp-sensornest](https://github.com/joaowinkelmann/esp-sensornest) | ESP-IDF firmware: sensor nodes + controller, mesh networking, Wi-Fi provisioning portal |
| `api/` | [api-sensornest](https://github.com/joaowinkelmann/api-sensornest) | NestJS backend: auth, persistence, statistics, map tiling, uploads |
| `web/` | [front-sensornest](https://github.com/joaowinkelmann/front-sensornest) | Vue 3 admin: spatial visualization, map editor, reports |

```
sensor nodes (ESP32) --ESP-NOW / Wi-Fi Mesh--> controller (ESP32) --HTTPS--> api/ --Postgres/OCI--> web/
```

## Objectives

- Study WSNs, mesh topologies, low-power protocols (ESP32-C3), data encryption, user auth, persistence and spatial visualization.
- Build node + controller hardware/firmware with Mesh comms, low power draw and encrypted collection/transmission.
- Build backend (`api/`) for auth, user/device management and cloud persistence, plus front-end (`web/`) for interactive map visualization with access control.
- Validate efficiency, scalability and security: energy tests, mesh range, encrypted-transmission reliability, visualization + access control.

## firmware/

ESP-IDF (C). Targets include ESP32, ESP32-C3, ESP32-S3 (energy stress test ran on ESP32-S3 @ 80 MHz).

- `main/src/app/sensornest.c` — entry point; `roles/` — `controller_role`, `worker_role`, `role_manager`
- `net/` — `espnow_manager`, `wifi_manager`, `rest_client`, `rest_server`, `captive_dns`
- `sensor/` — sensor readout (DHT via `esp-idf-lib/dht`); `storage/` (LittleFS), `security/`, `indicator/` (LED strip), `front/` — Wi-Fi provisioning portal
- Provisioning flow: first boot with no saved credentials starts open AP `ESP-{shortID}` with captive portal (`index.html`); user connects by phone/PC, enters nearby Wi-Fi credentials and logs into the API; device registers to that user via POST, then leaves setup and works as collector.

```bash
cd firmware
idf.py set-target esp32s3   # or esp32c3
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

Energy notes (from TCC research): AMS1117 regulators waste ~4–5 mA quiescent — avoid for battery nodes; prefer efficient SOT-23-5 LDOs and power switching. See Notion TCC page for full test logs.

## api/

NestJS 11 + Fastify + Prisma 7 + Postgres. Global prefix `sensornest/api/`, Swagger at `sensornest/api/docs`. JWT auth enforced globally (`AuthGuard`), mail via OCI Email Delivery (Handlebars templates).

Modules (`api/src/`): `auth`, `users`, `meshes`, `zones`, `devices`, `readings`, `statistics` (per-mesh/zone/device metrics: highs, averages over day/week/month/year), `data-exporter`, `uploader` (multipart, 5 MB max), `tiler` (map tiles), `device-checker` (flags inactive devices), `health`, `mailer`.

Data model (`api/prisma/schema.prisma`): `User`, `Mesh`, `Zone`, `Device`, `Reading` (+ migrations, `seed.ts`).

```bash
cd api
cp .env.template .env   # fill DATABASE_URL, JWT_SECRET, OCI_* (storage bucket sensornest-maps, email, map tiling)
npm install
npx prisma migrate dev
npm run start:dev
```

Key env (`api/.env.template`): `DATABASE_URL`, `APP_PORT`, `APP_BASE_URL`, `JWT_SECRET`, `JWT_EXPIRES_IN`, `OCI_REGION/TENANCY/USER/FINGERPRINT/PRIVATE_KEY(_PATH)/NAMESPACE/BUCKET`, `OCI_EMAIL_*`, `MAP_TILE_FORMAT/QUALITY/EFFORT/MAX_ZOOM`, `TIME_BEFORE_DEVICE_INACTIVE`.

## web/

Vue 3 + Quasar 2 + Vite 7 + Pinia + vue-router + axios. Maps: Leaflet + leaflet-draw (zone editor, 3+ vertices to save). Charts: ECharts. i18n: PT/EN.

Routes (`web/src/views/`): `auth/` (login, register, reset-password), `home/`, `meshes/`, `zones/`, `devices/`, `data/`, `map-editor/`, `reports/`.

```bash
cd web
cp .env.template .env   # VITE_API_URL=https://<host>/sensornest/api/ VITE_PORT VITE_OCI_MAP_TOKEN
npm install
npm run dev     # preview: npm run preview | serve: npm start (pm2, port 7772)
npm test        # vitest
```

## History

Merged with `git subtree add --prefix=<dir> <source> main` per component, so `git log --all --graph` shows every original commit. Path-filtered log (`git log -- web/...`) shows one commit per prefix — expected for subtree merges; use `git log --merges` + second parents for archaeology.

## Academic context

Full TCC notes (monograph PDF, energy analyses, provisioning flow sketches, API docs draft) live in the private Notion page `[n] SensorNest (TCC)`. Public portfolio surface is this repo.
