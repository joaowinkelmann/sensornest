import { Injectable, Logger } from '@nestjs/common';
import { ConfigService } from '@nestjs/config';
import { Cron, CronExpression } from '@nestjs/schedule';
import { PrismaService } from '../../prisma/prisma.service';
import { DeviceStatus } from '@prisma/client';

@Injectable()
export class DeviceCheckerService {
  private readonly logger = new Logger(DeviceCheckerService.name);

  constructor(
    private readonly prisma: PrismaService,
    private readonly configService: ConfigService,
  ) { }

  // Helper para interpretar strings de tempo como "1 day", "2 hours", etc. (como o strtotime do PHP)
  private parseTimeString(timeString: string): number {
    const [value, unit] = timeString.split(' ');
    const intValue = parseInt(value, 10);
    switch (unit) {
      case 'day':
      case 'days':
        return intValue * 24 * 60 * 60 * 1000;
      case 'hour':
      case 'hours':
        return intValue * 60 * 60 * 1000;
      case 'minute':
      case 'minutes':
        return intValue * 60 * 1000;
      case 'second':
      case 'seconds':
        return intValue * 1000;
      default:
        return 0;
    }
  }


  // uma vez a cada 30 minutos
  @Cron(CronExpression.EVERY_30_MINUTES)
  async handleCron() {
    this.logger.debug('Verificando dispositivos inativos...');

    const timeBeforeInactiveString = this.configService.get<string>(
      'TIME_BEFORE_DEVICE_INACTIVE',
      '1 day', // Padrão de 1 dia
    );
    const timeBeforeInactive = this.parseTimeString(timeBeforeInactiveString);
    const cutoffDate = new Date(Date.now() - timeBeforeInactive);

    const devices = await this.prisma.device.findMany({
      where: {
        status: DeviceStatus.ACTIVE,
      },
      include: {
        readings: {
          orderBy: {
            readingTime: 'desc',
          },
          take: 1,
        },
      },
    });

    for (const device of devices) {
      if (device.readings.length > 0) {
        const lastReadingTime = device.readings[0].readingTime;
        if (lastReadingTime < cutoffDate) {
          await this.prisma.device.update({
            where: { id: device.id },
            data: { status: DeviceStatus.INACTIVE },
          });
          this.logger.log(
            `Device ${device.name} (${device.id}) marked as inactive.`,
          );
        }
      } else {
        // Se o dispositivo não tiver leituras, verifica a data de criação
        if (device.insDthr < cutoffDate) {
          await this.prisma.device.update({
            where: { id: device.id },
            data: { status: DeviceStatus.INACTIVE },
          });
          this.logger.log(
            `Device ${device.name} (${device.id}) with no readings marked as inactive.`,
          );
        }
      }
    }
  }
}
