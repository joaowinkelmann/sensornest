import { Injectable, NotFoundException } from '@nestjs/common';
import { PrismaService } from '../../prisma/prisma.service';
import { subDays, subWeeks, subMonths, subYears } from 'date-fns';

@Injectable()
export class StatisticsService {
  constructor(private readonly prisma: PrismaService) {}

  private async getStatistics(
    model: 'device' | 'zone' | 'mesh',
    id: string,
  ) {
    const now = new Date();
    const periods = {
      day: subDays(now, 1),
      week: subWeeks(now, 1),
      month: subMonths(now, 1),
      year: subYears(now, 1),
    };

    const whereCondition =
      model === 'device'
        ? { deviceId: id }
        : model === 'zone'
          ? { device: { zoneId: id } }
          : { device: { meshId: id } };

    const queries = Object.entries(periods).map(async ([period, date]) => {
      const result = await this.prisma.reading.aggregate({
        where: {
          ...whereCondition,
          readingTime: { gte: date },
        },
        _max: { temperature: true, humidity: true },
        _avg: { temperature: true, humidity: true },
      });
      return [period, result];
    });

    const results = await Promise.all(queries);
    return results.reduce((acc, [period, result]) => {
      acc[period] = {
        highestTemperature: result._max.temperature,
        highestHumidity: result._max.humidity,
        averageTemperature: result._avg.temperature,
        averageHumidity: result._avg.humidity,
      };
      return acc;
    }, {});
  }

  async getDeviceStatistics(deviceId: string) {
    const device = await this.prisma.device.findUnique({
      where: { id: deviceId },
    });
    if (!device) {
      throw new NotFoundException('Device not found');
    }
    return this.getStatistics('device', deviceId);
  }

  async getZoneStatistics(zoneId: string) {
    const zone = await this.prisma.zone.findUnique({ where: { id: zoneId } });
    if (!zone) {
      throw new NotFoundException('Zone not found');
    }
    return this.getStatistics('zone', zoneId);
  }

  async getMeshStatistics(meshId: string) {
    const mesh = await this.prisma.mesh.findUnique({ where: { id: meshId } });
    if (!mesh) {
      throw new NotFoundException('Mesh not found');
    }
    return this.getStatistics('mesh', meshId);
  }
}
