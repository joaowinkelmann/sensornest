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
  ) {}

  // A helper function to parse time strings like "1 day", "2 hours", etc.
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

  // @Cron(CronExpression.EVERY_DAY_AT_MIDNIGHT)
  // uma vez a cada 30 minutos
  @Cron(CronExpression.EVERY_30_MINUTES)
  async handleCron() {
    this.logger.debug('Verificando dispositivos inativos...');

    const timeBeforeInactiveString = this.configService.get<string>(
      'TIME_BEFORE_DEVICE_INACTIVE',
      '1 day', // Default to 1 day
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
        // If device has no readings, check its creation date
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
