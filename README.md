# sensornest

Unified monorepo for the SensorNest TCC project (portfolio mirror).
History preserved via `git subtree` merges. Original repos stay up as-is.

| Dir | From | Stack |
|-----|------|-------|
| `firmware/` | [esp-sensornest](https://github.com/joaowinkelmann/esp-sensornest) | ESP-IDF (C), sensor firmware |
| `api/` | [api-sensornest](https://github.com/joaowinkelmann/api-sensornest) | NestJS + Prisma + Postgres |
| `web/` | [front-sensornest](https://github.com/joaowinkelmann/front-sensornest) | Vue 3 + Quasar + Vite |

## Dev

```bash
# firmware
cd firmware && idf.py build
# api
cd api && npm install && npm run start:dev
# web
cd web && npm install && npm run dev
```

Full project context (TCC) will be folded in from Notion next.
