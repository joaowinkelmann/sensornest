import { PrismaClient, DeviceStatus, DeviceRole } from '@prisma/client';
import * as argon2 from 'argon2';

const prisma = new PrismaClient();

async function main() {
  console.log('🌱 Iniciando seed...');

  // 1. Usuário de teste
  const hashedPassword = await argon2.hash('password123');
  const user = await prisma.user.upsert({
    where: { email: 'test@example.com' },
    update: {
      passwordHash: hashedPassword,
    },
    create: {
      email: 'test@example.com',
      name: 'Kléber da Silva Santiago Azevêdo',
      passwordHash: hashedPassword,
    },
  });
  console.log('✅ Criado usuário:', user.email);

  // 2. Cria um mesh pro cara
  const mesh = await prisma.mesh.upsert({
    where: { id: 'mesh-1' },
    update: {},
    create: {
      id: 'mesh-1',
      name: 'Mesh dos Guris',
      userId: user.id,
      lat: -23.5505, // Latitude do centro da Mesh
      lon: -46.6333, // Longitude do centro da Mesh
    },
  });
  console.log('✅ Criada mesh:', mesh.name);

  // 3. Cria uma Zone dentro dessa Mesh
  const zone = await prisma.zone.upsert({
    where: { id: 'zone-1' },
    update: {},
    create: {
      id: 'zone-1',
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
  console.log('✅ Criada zone:', zone.name);

  // 4. Cria um controlador ativo para a zone (Device com role CONTROLLER)
  const controller = await prisma.device.upsert({
    where: { macAddress: '24:58:7C:CC:E5:B4' },
    update: {},
    create: {
      macAddress: '24:58:7C:CC:E5:B4',
      name: 'Luzes de Natal',
      description: 'Controlador principal com acesso à rede AP',
      x: 5,
      y: 5,
      zoneId: zone.id,
      status: DeviceStatus.ACTIVE,
      role: DeviceRole.CONTROLLER,
      userId: user.id,
      meshId: mesh.id,
      deviceColor: '#FF0000',
      readingsPerBatch: 2,
      wakeUpInterval: 60 * 60, // 1 hora
    },
  });
  console.log(`✅ Criado dispositivo controller ACTIVE: ${controller.name}`);

  // 5. Cria workers (Device com role WORKER)
  const activeWorker = await prisma.device.upsert({
    where: { macAddress: 'AA:BB:CC:DD:EE:01' },
    update: {},
    create: {
      macAddress: 'AA:BB:CC:DD:EE:01',
      name: 'Sensor - Sala de Reuniões',
      description: 'Sensor de temperatura e umidade na sala de reuniões',
      x: 50,
      y: 25,
      zoneId: zone.id,
      status: DeviceStatus.ACTIVE,
      role: DeviceRole.WORKER,
      userId: user.id,
      meshId: mesh.id,
      deviceColor: '#00FF00',
      readingsPerBatch: 1,
      wakeUpInterval: 60 * 30, // 30 minutos
    },
  });
  console.log(`✅ Criado dispositivo worker ACTIVE: ${activeWorker.name}`);

  const pendingWorker = await prisma.device.upsert({
    where: { macAddress: 'AA:BB:CC:DD:EE:02' },
    update: {},
    create: {
      macAddress: 'AA:BB:CC:DD:EE:02',
      name: 'Sensor ainda não configurado',
      description: 'Dispositivo recém descoberto pela rede',
      x: 0,
      y: 0,
      status: DeviceStatus.PENDING,
      role: DeviceRole.WORKER,
      userId: user.id,
      meshId: mesh.id,
      deviceColor: '#0000FF',
      readingsPerBatch: 1,
      wakeUpInterval: 60 * 60 * 2, // 2 horas
    },
  });
  console.log(`✅ Criado dispositivo worker PENDING device: ${pendingWorker.name}`);

  // 6. Cria leituras de exemplo para o worker ativo
  const readings: any[] = [];
  const now = Date.now();
  const daysToSeed = 14; // Seed 14 days
  const readingsPerDay = 4; // 4 readings per day (e.g., 00h, 06h, 12h, 18h)
  for (let day = 0; day < daysToSeed; day++) {
    for (let hourIdx = 0; hourIdx < readingsPerDay; hourIdx++) {
      const hour = hourIdx * 6; // 0, 6, 12, 18
      const readingTime = new Date(
        now - day * 24 * 60 * 60 * 1000 - hour * 60 * 60 * 1000
      );
      const temperature = 20 + Math.random() * 10; // 20-30°C
      const humidity = 40 + Math.random() * 30; // 40-70%
      const reading = await prisma.reading.create({
        data: {
          temperature,
          humidity,
          readingTime,
          deviceId: activeWorker.id,
        },
      });
      readings.push(reading);
    }
  }
  console.log(
    `✅ Criado ${readings.length} leituras distribuídas em ${daysToSeed} dias para o dispositivo ${activeWorker.name}.`
  );

  console.log('\n🎉 Seed completado com sucesso!');
  console.log('📧 Credenciais do usuário de teste:');
  console.log('   Email: test@example.com');
  console.log('   Password: password123');
}

main()
  .then(async () => {
    await prisma.$disconnect();
  })
  .catch(async (e) => {
    console.error('❌ Seed falhou:', e);
    await prisma.$disconnect();
    process.exit(1);
  });
