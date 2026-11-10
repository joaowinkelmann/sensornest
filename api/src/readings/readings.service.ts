import { Injectable, Logger } from '@nestjs/common';
import { PrismaService } from '../../prisma/prisma.service';
import { CreateReadingsDto } from './dto/create-readings.dto';

@Injectable()
export class ReadingsService {
  private readonly logger = new Logger(ReadingsService.name);

  constructor(private prisma: PrismaService) {}

  async createReadings(readings: CreateReadingsDto) {
    // 1. Pega os MAC únicos do json recebido
    const macAddresses = [...new Set(readings.data.map((r) => r.mac))];
    this.logger.log(
      `Recebido dados dos seguintes MACs: ${macAddresses.join(', ')}`,
    );

    // 2. Encontra os dispositivos com esses endereços no banco
    const foundDevices = await this.prisma.device.findMany({
      where: {
        macAddress: {
          in: macAddresses,
        },
      },
      select: {
        id: true,
        macAddress: true,
      },
    });

    // 3. Faz um mapa do mac para o worker
    const foundDevicesIdMap = new Map(
      foundDevices.map((w) => [w.macAddress, w.id]),
    );
    this.logger.log(
      `Encontrados ${foundDevicesIdMap.size} dispositivos registrados no banco.`,
    );

    // 4. Monta os dados pra inserir no banco
    const readingsToCreate = readings.data
      .map((reading) => {
        const foundDeviceId = foundDevicesIdMap.get(reading.mac);
        if (!foundDeviceId) {
          // Se não está registrado ainda, ignora
          this.logger.warn(
            `Ignorando dados de MAC não registrado: ${reading.mac}`,
          );
          return null;
        }
        return {
          temperature: reading.temp,
          humidity: reading.hum,
          readingTime: new Date(),
          deviceId: foundDeviceId,
        };
      })
      .filter((r) => r !== null);

    if (readingsToCreate.length === 0) {
      this.logger.log(
        'Nenhum dado válido para inserir. Todos os MACs não registrados.',
      );
      return { createdCount: 0, unregisteredCount: readings.data.length };
    }

    // 5. Inserindo em massa
    const result = await this.prisma.reading.createMany({
      data: readingsToCreate,
    });

    this.logger.log(`Foram criados ${result.count} registros de leitura.`);

    return {
      createdCount: result.count,
      unregisteredCount: readings.data.length - result.count,
    };
  }

  async remove(id: string) {
    await this.prisma.reading.delete({
      where: { id },
    });
  }
}
