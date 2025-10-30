import { Test, TestingModule } from '@nestjs/testing';
import {
  FastifyAdapter,
  NestFastifyApplication,
} from '@nestjs/platform-fastify';
import { ValidationPipe } from '@nestjs/common';
import request from 'supertest';
import { AppModule } from '../src/app.module';
import { PrismaClient, DeviceStatus, DeviceRole } from '@prisma/client';
import * as argon2 from 'argon2';
import { promises as fsPromises, existsSync } from 'node:fs';
import * as path from 'node:path';
import * as crypto from 'node:crypto';
import sharp from 'sharp';
import { MailerService } from '../src/mailer/mailer.service';

describe('App E2E', () => {
  let app: NestFastifyApplication;
  let prisma: PrismaClient;
  let accessToken: string;

  // --- Constantes e Helpers do SensorNest ---
  const runId = crypto.randomUUID().replace(/-/g, '');
  const buildMac = (hex: string) =>
    hex
      .padEnd(12, '0')
      .slice(0, 12)
      .toUpperCase()
      .match(/.{1,2}/g)!
      .join(':');

  // Constantes de seed (SensorNest)
  const userEmail = `e2e+${runId}@example.com`;
  const userPassword = `testsuitepassword189!${runId.slice(0, 6)}`;
  const userName = 'E2E Test Runner';
  const meshId = `mesh-${runId}`;
  const zoneId = `zone-${runId}`;
  const statsTestMeshId = `stats-mesh-${runId}`; // New mesh for stats tests
  const statsTestZoneId = `stats-zone-${runId}`; // New zone for stats tests
  const meshUploadId = `mesh-upload-${runId}`;
  const controllerMac = buildMac(runId.slice(0, 12));
  const knownWorkerMac = buildMac(runId.slice(12, 24));
  const statsTestMac = buildMac(runId.slice(24, 36)); // New device for stats tests

  const newUserEmail = `e2e+new-${runId}@example.com`;
  const newUserPassword = `newtestsuitepassword189!${runId.slice(0, 6)}`;
  const newUserName = 'New E2E User';

  // Seed (SensorNest)
  const seed = async () => {
    const hashedPassword = await argon2.hash(userPassword);
    const user = await prisma.user.upsert({
      where: { email: userEmail },
      update: { passwordHash: hashedPassword },
      create: {
        email: userEmail,
        name: userName,
        passwordHash: hashedPassword,
      },
    });

    const mesh = await prisma.mesh.upsert({
      where: { id: meshId },
      update: {},
      create: {
        id: meshId,
        name: 'Mesh dos Guris',
        userId: user.id,
        lat: -23.5505,
        lon: -46.6333,
      },
    });

    const zone = await prisma.zone.upsert({
      where: { id: zoneId },
      update: {},
      create: {
        id: zoneId,
        name: 'Planalto Central',
        meshId: mesh.id,
        vertices: [
          { x: 0, y: 0 },
          { x: 100, y: 0 },
          { x: 100, y: 50 },
          { x: 0, y: 50 },
        ],
      },
    });

    await prisma.device.upsert({
      where: { macAddress: controllerMac },
      update: {},
      create: {
        macAddress: controllerMac,
        name: 'Luzes de Natal',
        description: 'Controller device',
        x: 5,
        y: 5,
        meshId: mesh.id,
        zoneId: zone.id,
        status: DeviceStatus.ACTIVE,
        role: DeviceRole.CONTROLLER,
        userId: user.id,
        deviceColor: '#FF0000',
        readingsPerBatch: 2,
        wakeUpInterval: 3600,
      },
    });

    await prisma.device.upsert({
      where: { macAddress: knownWorkerMac },
      update: {},
      create: {
        macAddress: knownWorkerMac,
        name: 'Sensor - Sala de Reuniões',
        description: 'Worker sensor',
        x: 50,
        y: 25,
        meshId: mesh.id,
        zoneId: zone.id,
        status: DeviceStatus.ACTIVE,
        role: DeviceRole.WORKER,
        userId: user.id,
        deviceColor: '#00FF00',
        readingsPerBatch: 1,
        wakeUpInterval: 1800,
      },
    });

    // Seed rich readings for the worker device
    const workerDevice = await prisma.device.findUnique({
      where: { macAddress: knownWorkerMac },
    });
    const daysToSeed = 14;
    const readingsPerDay = 4;
    const now = Date.now();
    for (let day = 0; day < daysToSeed; day++) {
      for (let hourIdx = 0; hourIdx < readingsPerDay; hourIdx++) {
        const hour = hourIdx * 6; // 0, 6, 12, 18
        const readingTime = new Date(
          now - day * 24 * 60 * 60 * 1000 - hour * 60 * 60 * 1000
        );
        const temperature = 20 + Math.random() * 10; // 20-30°C
        const humidity = 40 + Math.random() * 30; // 40-70%
        await prisma.reading.create({
          data: {
            temperature,
            humidity,
            readingTime,
            deviceId: workerDevice!.id,
          },
        });
      }
    }

    // Create separate mesh and zone for stats tests
    const statsMesh = await prisma.mesh.upsert({
      where: { id: statsTestMeshId },
      update: {},
      create: {
        id: statsTestMeshId,
        name: 'Stats Test Mesh',
        userId: user.id,
        lat: -23.552,
        lon: -46.635,
      },
    });

    const statsZone = await prisma.zone.upsert({
      where: { id: statsTestZoneId },
      update: {},
      create: {
        id: statsTestZoneId,
        name: 'Stats Test Zone',
        meshId: statsMesh.id,
        vertices: [
          { x: 0, y: 0 },
          { x: 50, y: 0 },
          { x: 50, y: 50 },
          { x: 0, y: 50 },
        ],
      },
    });

    // Create a separate device for statistics tests with NO initial readings
    await prisma.device.upsert({
      where: { macAddress: statsTestMac },
      update: {},
      create: {
        macAddress: statsTestMac,
        name: 'Stats Test Device',
        description: 'Device used for statistics E2E tests',
        x: 25,
        y: 25,
        meshId: statsMesh.id,
        zoneId: statsZone.id,
        status: DeviceStatus.ACTIVE,
        role: DeviceRole.WORKER,
        userId: user.id,
        deviceColor: '#0000FF',
        readingsPerBatch: 1,
        wakeUpInterval: 1800,
      },
    });

    await prisma.mesh.create({
      data: {
        id: meshUploadId,
        name: 'Mesh Upload Target',
        userId: user.id,
        lat: -23.551,
        lon: -46.634,
      },
    });
  };

  const fixturesDir = path.resolve(__dirname, 'fixtures');

  // --- Configuração e Desmontagem Global ---

  beforeAll(async () => {
    process.env.JWT_SECRET = process.env.JWT_SECRET || 'test-secret';
    process.env.JWT_EXPIRES_IN = process.env.JWT_EXPIRES_IN || '3600s';

    prisma = new PrismaClient();
    await prisma.$connect();
    await seed();

    const moduleFixture: TestingModule = await Test.createTestingModule({
      imports: [AppModule],
    })
      .overrideProvider(MailerService) // Mock do MailerService (do arquivo 2)
      .useValue({
        sendPasswordRecoveryEmail: jest.fn().mockResolvedValue(undefined),
      })
      .compile();

    app = moduleFixture.createNestApplication<NestFastifyApplication>(
      new FastifyAdapter(), // Fastify (do arquivo 1)
    );

    // Pipe de Validação (do arquivo 2)
    app.useGlobalPipes(
      new ValidationPipe({ whitelist: true, forbidNonWhitelisted: true }),
    );

    await app.init();
    await app.getHttpAdapter().getInstance().ready();
  }, 60000); // Timeout de 60s (do arquivo 1)

  afterAll(async () => {
    // Teardown
    const user = await prisma.user.findUnique({ where: { email: userEmail } });
    if (user) {
      await prisma.reading.deleteMany({ where: { device: { userId: user.id } } });
      await prisma.device.deleteMany({ where: { userId: user.id } });
      await prisma.zone.deleteMany({
        where: { meshId: { in: [meshId, statsTestMeshId, meshUploadId] } },
      });
      await prisma.mesh.deleteMany({
        where: { id: { in: [meshId, statsTestMeshId, meshUploadId] } },
      });
      await prisma.user.delete({ where: { id: user.id } });
    }

    const newUser = await prisma.user.findUnique({
      where: { email: newUserEmail },
    });
    if (newUser) {
      await prisma.user.delete({ where: { email: newUserEmail } });
    }

    // Teardown (do arquivo 2)
    await prisma.user.deleteMany({ where: { email: 'e2e-reset@example.com' } });

    // Fechamento global
    await app.close();
    await prisma.$disconnect();
    await fsPromises.rm(fixturesDir, { recursive: true, force: true });
  });

  // --- Testes do SensorNest ---

  describe('SensorNest Core', () => {
    const sampleImagePath = path.join(fixturesDir, 'sample_image.webp');

    beforeAll(async () => {
      // Criação de fixtures (do arquivo 1)
      await fsPromises.mkdir(fixturesDir, { recursive: true });
      if (!existsSync(sampleImagePath)) {
        const buffer = await sharp({
          create: {
            width: 128,
            height: 128,
            channels: 3,
            background: { r: 255, g: 0, b: 0 },
          },
        })
          .webp()
          .toBuffer();
        await fsPromises.writeFile(sampleImagePath, buffer);
      }
    });

    it('redireciona a raiz (/) para a documentação (302)', async () => {
      const res = await request(app.getHttpServer()).get('/');
      expect([301, 302]).toContain(res.status);
    });

    it('autentica e obtém token JWT', async () => {
      const res = await request(app.getHttpServer())
        .post('/auth/login')
        .send({ email: userEmail, password: userPassword })
        .expect(200);
      expect(res.body).toHaveProperty('access_token');
      accessToken = res.body.access_token;
    });

    it('lista os dispositivos do usuário', async () => {
      const res = await request(app.getHttpServer())
        .get('/devices')
        .set('Authorization', `Bearer ${accessToken}`)
        .expect(200);
      expect(Array.isArray(res.body)).toBe(true);
      const worker = res.body.find((d: any) => d.macAddress === knownWorkerMac);
      expect(worker).toBeDefined();
    });

    it('adiciona leituras de sensores (1 válido, 1 MAC não registrado)', async () => {
      const payload = {
        data: [
          { mac: knownWorkerMac, temp: 25.4, hum: 60.1 },
          { mac: 'FF:00:11:22:33:44', temp: 30.2, hum: 55.5 },
        ],
      };
      const res = await request(app.getHttpServer())
        .post('/readings/add')
        .set('Authorization', `Bearer ${accessToken}`)
        .send(payload)
        .expect(201);
      expect(res.body).toMatchObject({
        createdCount: 1,
        unregisteredCount: 1,
      });
    });

    it('remove uma leitura pelo ID', async () => {
      const readings = await prisma.reading.findMany({
        where: {
          device: {
            macAddress: knownWorkerMac,
          },
        },
        orderBy: { readingTime: 'desc' },
        take: 1,
      });
      expect(readings.length).toBe(1);
      const readingId = readings[0].id; // get the ID of the most recent reading
      await request(app.getHttpServer())
        .delete(`/readings/${readingId}`)
        .set('Authorization', `Bearer ${accessToken}`)
        .expect(200, { message: 'Leitura deletada com sucesso.' });
    });

    it('busca as Zones de uma Mesh', async () => {
      const res = await request(app.getHttpServer())
        .get(`/meshes/${meshId}/zones`)
        .set('Authorization', `Bearer ${accessToken}`)
        .expect(200);
      expect(Array.isArray(res.body)).toBe(true);
      const zone = res.body.find((z: any) => z.id === zoneId);
      expect(zone).toBeDefined();
    });

    it('atualiza o nome de uma Zone', async () => {
      const newName = 'Planalto Central Atualizado';
      const res = await request(app.getHttpServer())
        .patch(`/zones/${zoneId}`)
        .set('Authorization', `Bearer ${accessToken}`)
        .send({ name: newName })
        .expect(200);
      expect(res.body.name).toBe(newName);
    });

    it('encontra dispositivos pela Zone', async () => {
      const res = await request(app.getHttpServer())
        .get(`/zones/${zoneId}/devices`)
        .set('Authorization', `Bearer ${accessToken}`)
        .expect(200);
      expect(Array.isArray(res.body)).toBe(true);
      expect(res.body.some((d: any) => d.macAddress === knownWorkerMac)).toBe(
        true,
      );
    });

    it('busca dispositivos pela Mesh', async () => {
      const res = await request(app.getHttpServer())
        .get(`/meshes/${meshId}/devices`)
        .set('Authorization', `Bearer ${accessToken}`)
        .expect(200);
      expect(res.body.map((d: any) => d.meshId)).toContain(meshId);
    });

    it('encontra um dispositivo pelo MAC', async () => {
      const res = await request(app.getHttpServer())
        .get(`/devices/mac/${controllerMac}`)
        .set('Authorization', `Bearer ${accessToken}`)
        .expect(200);
      expect(res.body).toMatchObject({ macAddress: controllerMac });
    });

    // it('uploads a custom map and persists map metadata', async () => {
    //   const FormData = require('form-data');
    //   const fs = require('fs');
    //   const form = new FormData();

    //   // Read file into buffer first
    //   const fileBuffer = await fsPromises.readFile(sampleImagePath);

    //   form.append('file', fileBuffer, {
    //     filename: 'sample_image.webp',
    //     contentType: 'image/webp',
    //   });
    //   form.append('meshId', meshUploadId);

    //   const fastifyInstance = app.getHttpAdapter().getInstance();

    //   // Get form buffer using the callback-based getBuffer method
    //   const formBuffer = await new Promise<Buffer>((resolve, reject) => {
    //     form.getBuffer((err: Error | null, buffer: Buffer) => {
    //       if (err) reject(err);
    //       else resolve(buffer);
    //     });
    //   });

    //   const response = await fastifyInstance.inject({
    //     method: 'POST',
    //     url: '/meshes/upload-map',
    //     headers: {
    //       ...form.getHeaders(),
    //       'authorization': `Bearer ${accessToken}`
    //     },
    //     payload: formBuffer
    //   });

    //   expect([200, 201]).toContain(response.statusCode);
    //   const body = JSON.parse(response.body);
    //   expect(body).toMatchObject({
    //     mapMinZoom: expect.any(Number),
    //     mapMaxZoom: expect.any(Number),
    //     mapUrl: expect.any(String),
    //     prefix: expect.any(String),
    //     ext: expect.any(String),
    //   });
    //   const mesh = await prisma.mesh.findUnique({
    //     where: { id: meshUploadId },
    //   });
    //   expect(mesh?.mapUrl).toBe(body.mapUrl);
    // }, 10000); // Increase timeout to 10 seconds

    it('registra um novo usuário', async () => {
      const res = await request(app.getHttpServer())
        .post('/users/register')
        .send({
          email: newUserEmail,
          password: newUserPassword,
          name: newUserName,
        })
        .expect(201);

      expect(res.body).toMatchObject({
        message: 'User created successfully',
        user: expect.objectContaining({
          email: newUserEmail,
          name: newUserName,
        }),
      });
    });

    describe('E2E Estatísticas', () => {
      let deviceId: string;
      let statsZoneIdForTest: string;
      let statsMeshIdForTest: string;

      // Define alguns valores para realizar os testes
      const temps = [20.0, 26.0, 34.2];
      const hums = [40.0, 65.0, 55.0];

      const expectedMaxTemp = Math.max(...temps);
      const expectedAvgTemp =
        temps.reduce((a, b) => a + b, 0) / temps.length;

      const expectedMaxHum = Math.max(...hums);
      const expectedAvgHum =
        hums.reduce((a, b) => a + b, 0) / hums.length;

      beforeAll(async () => {
        // Use the dedicated stats test device instead of knownWorkerMac
        await request(app.getHttpServer())
          .post('/readings/add')
          .set('Authorization', `Bearer ${accessToken}`)
          .send({
            data: temps.map((t, i) => ({
              mac: statsTestMac,
              temp: t,
              hum: hums[i],
            })),
          })
          .expect(201);

        const res = await request(app.getHttpServer())
          .get(`/devices/mac/${statsTestMac}`)
          .set('Authorization', `Bearer ${accessToken}`)
          .expect(200);

        expect(res.body).toHaveProperty('id');
        deviceId = res.body.id;
        statsZoneIdForTest = res.body.zoneId;
        statsMeshIdForTest = res.body.meshId;
      });

      it('calcula estatísticas do Dispositivo (max/avg)', async () => {
        const res = await request(app.getHttpServer())
          .get(`/statistics/device/${deviceId}`)
          .set('Authorization', `Bearer ${accessToken}`)
          .expect(200);

        const body = res.body;
        expect(body).toHaveProperty('day');
        const day = body.day;

        expect(day.highestTemperature).toBeCloseTo(expectedMaxTemp, 5);
        expect(day.averageTemperature).toBeCloseTo(expectedAvgTemp, 5);

        if (typeof day.highestHumidity === 'number') {
          expect(day.highestHumidity).toBeCloseTo(expectedMaxHum, 5);
        }
        if (typeof day.averageHumidity === 'number') {
          expect(day.averageHumidity).toBeCloseTo(expectedAvgHum, 5);
        }
      });

      it('calcula estatísticas da Zone (max/avg)', async () => {
        const res = await request(app.getHttpServer())
          .get(`/statistics/zone/${statsZoneIdForTest}`) // Use statsZoneIdForTest
          .set('Authorization', `Bearer ${accessToken}`)
          .expect(200);

        const body = res.body;
        expect(body).toHaveProperty('day');
        const day = body.day;

        expect(day.highestTemperature).toBeCloseTo(expectedMaxTemp, 5);
        expect(day.averageTemperature).toBeCloseTo(expectedAvgTemp, 5);

        if (typeof day.highestHumidity === 'number') {
          expect(day.highestHumidity).toBeCloseTo(expectedMaxHum, 5);
        }
        if (typeof day.averageHumidity === 'number') {
          expect(day.averageHumidity).toBeCloseTo(expectedAvgHum, 5);
        }
      });

      it('calcula estatísticas da Mesh (max/avg)', async () => {
        const res = await request(app.getHttpServer())
          .get(`/statistics/mesh/${statsMeshIdForTest}`) // Use statsMeshIdForTest
          .set('Authorization', `Bearer ${accessToken}`)
          .expect(200);

        const body = res.body;
        expect(body).toHaveProperty('day');
        const day = body.day;

        expect(day.highestTemperature).toBeCloseTo(expectedMaxTemp, 5);
        expect(day.averageTemperature).toBeCloseTo(expectedAvgTemp, 5);

        if (typeof day.highestHumidity === 'number') {
          expect(day.highestHumidity).toBeCloseTo(expectedMaxHum, 5);
        }
        if (typeof day.averageHumidity === 'number') {
          expect(day.averageHumidity).toBeCloseTo(expectedAvgHum, 5);
        }
      });

      it('retorna overview de estatísticas do usuário', async () => {
        const res = await request(app.getHttpServer())
          .get('/statistics/overview')
          .set('Authorization', `Bearer ${accessToken}`)
          .expect(200);

        const body = res.body;

        // Verifica estrutura de devices
        expect(body).toHaveProperty('devices');
        expect(body.devices).toHaveProperty('active');
        expect(body.devices).toHaveProperty('inactive');
        expect(body.devices).toHaveProperty('total');
        expect(typeof body.devices.active).toBe('number');
        expect(typeof body.devices.inactive).toBe('number');
        expect(typeof body.devices.total).toBe('number');
        expect(body.devices.total).toBe(body.devices.active + body.devices.inactive);

        // Verifica que existem pelo menos 2 dispositivos (controller + worker)
        expect(body.devices.total).toBeGreaterThanOrEqual(2);

        // Verifica estrutura de readings
        expect(body).toHaveProperty('readings');
        expect(body.readings).toHaveProperty('allTime');
        expect(body.readings).toHaveProperty('lastHour');
        expect(body.readings).toHaveProperty('lastDay');
        expect(typeof body.readings.allTime).toBe('number');
        expect(typeof body.readings.lastHour).toBe('number');
        expect(typeof body.readings.lastDay).toBe('number');

        // Verifica que existem leituras (das que foram inseridas nos testes)
        expect(body.readings.allTime).toBeGreaterThanOrEqual(temps.length);

        // Verifica estrutura de readingsPerMesh
        expect(body).toHaveProperty('readingsPerMesh');
        expect(Array.isArray(body.readingsPerMesh)).toBe(true);
        
        // Verifica que o mesh do seed está presente
        const meshStats = body.readingsPerMesh.find(
          (m: any) => m.meshId === meshId
        );
        expect(meshStats).toBeDefined();
        expect(meshStats).toHaveProperty('meshName');
        expect(meshStats).toHaveProperty('count');
        expect(typeof meshStats.count).toBe('number');
      });

      it('retorna overview vazio para usuário sem dispositivos', async () => {
        // Cria um novo usuário temporário sem dispositivos
        const tempUserEmail = `temp-${runId}@example.com`;
        const tempUserPassword = 'TempPass123!';

        await request(app.getHttpServer())
          .post('/users/register')
          .send({
            email: tempUserEmail,
            password: tempUserPassword,
            name: 'Temp User',
          })
          .expect(201);

        // Faz login com o novo usuário
        const loginRes = await request(app.getHttpServer())
          .post('/auth/login')
          .send({ email: tempUserEmail, password: tempUserPassword })
          .expect(200);

        const tempAccessToken = loginRes.body.access_token;

        // Busca overview
        const res = await request(app.getHttpServer())
          .get('/statistics/overview')
          .set('Authorization', `Bearer ${tempAccessToken}`)
          .expect(200);

        const body = res.body;

        // Verifica que não tem dispositivos
        expect(body.devices.total).toBe(0);
        expect(body.devices.active).toBe(0);
        expect(body.devices.inactive).toBe(0);

        // Verifica que não tem leituras
        expect(body.readings.allTime).toBe(0);
        expect(body.readings.lastHour).toBe(0);
        expect(body.readings.lastDay).toBe(0);

        // Verifica que não tem meshes
        expect(body.readingsPerMesh).toEqual([]);

        // Limpa o usuário temporário
        const tempUser = await prisma.user.findUnique({
          where: { email: tempUserEmail },
        });
        if (tempUser) {
          await prisma.user.delete({ where: { id: tempUser.id } });
        }
      });

      it('falha ao buscar overview sem autenticação', async () => {
        await request(app.getHttpServer())
          .get('/statistics/overview')
          .expect(401);
      });

      it('retorna dados para gráficos do dispositivo', async () => {
        // Use knownWorkerMac which has 56 readings over 14 days
        const workerRes = await request(app.getHttpServer())
          .get(`/devices/mac/${knownWorkerMac}`)
          .set('Authorization', `Bearer ${accessToken}`)
          .expect(200);

        const workerDeviceId = workerRes.body.id;

        const res = await request(app.getHttpServer())
          .get(`/statistics/device/${workerDeviceId}/graph`)
          .set('Authorization', `Bearer ${accessToken}`)
          .expect(200);

        expect(res.body).toHaveProperty('day');
        expect(Array.isArray(res.body.day)).toBe(true);
        expect(res.body).toHaveProperty('week');
        expect(Array.isArray(res.body.week)).toBe(true);
        expect(res.body).toHaveProperty('month');
        expect(Array.isArray(res.body.month)).toBe(true);

        // Verify day has multiple buckets (4 readings per day, grouped by hour)
        expect(res.body.day.length).toBeGreaterThan(0);
        res.body.day.forEach((bucket: any) => {
          expect(bucket).toHaveProperty('bucket');
          expect(bucket).toHaveProperty('avgTemperature');
          expect(bucket).toHaveProperty('avgHumidity');
          expect(typeof bucket.avgTemperature).toBe('number');
          expect(typeof bucket.avgHumidity).toBe('number');
        });

        // Verify week has multiple buckets (14 days of data, grouped by day)
        expect(res.body.week.length).toBeGreaterThan(0);
        res.body.week.forEach((bucket: any) => {
          expect(bucket).toHaveProperty('bucket');
          expect(bucket).toHaveProperty('avgTemperature');
          expect(bucket).toHaveProperty('avgHumidity');
        });

        // Verify month has multiple buckets (14 days of data, grouped by day)
        expect(res.body.month.length).toBeGreaterThan(0);
        expect(res.body.month.length).toBeLessThanOrEqual(15); // Allow 14-15 days due to partial day boundaries
        res.body.month.forEach((bucket: any) => {
          expect(bucket).toHaveProperty('bucket');
          expect(bucket).toHaveProperty('avgTemperature');
          expect(bucket).toHaveProperty('avgHumidity');
        });
      });
    });
  });

  describe('E2E Reset Senha', () => {
    const testEmail = 'e2e-reset@example.com';
    const oldPassword = 'OldPass123!';
    const newPassword = 'NewPass123!';

    beforeEach(async () => {
      // Limpa e semeia um usuário novo para este teste
      await prisma.user.deleteMany({ where: { email: testEmail } });
      await prisma.user.create({
        data: {
          email: testEmail,
          name: 'E2E Reset',
          passwordHash: await argon2.hash(oldPassword),
        },
      });
    });

    const setResetTokenForUser = async (expiresInHours = 1) => {
      const token = crypto.randomBytes(20).toString('hex');
      const expires = new Date();
      expires.setHours(expires.getHours() + expiresInHours);

      await prisma.user.update({
        where: { email: testEmail },
        data: {
          passwordResetToken: token,
          passwordResetExpires: expires,
        },
      });

      return token;
    };

    it('GET /users/reset-password/check should accept a valid token', async () => {
      const token = await setResetTokenForUser(1);

      await request(app.getHttpServer())
        .get('/users/reset-password/check')
        .query({ token })
        .expect(200)
        .expect(({ body }) => {
          expect(body).toEqual({ valid: true });
        });
    });

    it('GET /users/reset-password/check should reject an invalid token', async () => {
      await setResetTokenForUser(1);

      await request(app.getHttpServer())
        .get('/users/reset-password/check')
        .query({ token: 'invalid-token' })
        .expect(400);
    });

    it('POST /users/reset-password should update password and clear token fields', async () => {
      const token = await setResetTokenForUser(1);

      await request(app.getHttpServer())
        .post('/users/reset-password')
        .send({ token, newPassword })
        .expect(200)
        .expect(({ body }) => {
          expect(body).toEqual({ message: 'Password updated successfully' });
        });

      const user = await prisma.user.findUnique({ where: { email: testEmail } });
      expect(user).toBeTruthy();
      expect(user?.passwordResetToken).toBeNull();
      expect(user?.passwordResetExpires).toBeNull();
      expect(await argon2.verify(user!.passwordHash, newPassword)).toBe(true);
    });

    it('POST /users/reset-password should reject expired token', async () => {
      const token = await setResetTokenForUser(-1); // expirado

      await request(app.getHttpServer())
        .post('/users/reset-password')
        .send({ token, newPassword })
        .expect(400);
    });
  });
});
