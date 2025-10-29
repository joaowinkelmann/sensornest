import { Injectable, NotFoundException } from '@nestjs/common';
import { PrismaService } from '../../prisma/prisma.service';
import { subDays, subWeeks, subMonths, subYears } from 'date-fns';

type PeriodKey = 'day' | 'week' | 'month' | 'year';

type ReadingAgg = {
  _max: { temperature: number | null; humidity: number | null };
  _avg: { temperature: number | null; humidity: number | null };
};

type Stats = {
  highestTemperature: number | null;
  highestHumidity: number | null;
  averageTemperature: number | null;
  averageHumidity: number | null;
};

@Injectable()
export class StatisticsService {
  constructor(private readonly prisma: PrismaService) {}

  private async getStatistics(model: 'device' | 'zone' | 'mesh', id: string) {
    const now = new Date();
    const periods: Record<PeriodKey, Date> = {
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

    const entries = Object.entries(periods) as [PeriodKey, Date][];

    const results = await Promise.all(
      entries.map(async ([period, date]) => {
        const result = await this.prisma.reading.aggregate({
          where: {
            ...whereCondition,
            readingTime: { gte: date },
          },
          _max: { temperature: true, humidity: true },
          _avg: { temperature: true, humidity: true },
        });
        return [period, result as ReadingAgg] as [PeriodKey, ReadingAgg];
      }),
    );

    const acc: Record<PeriodKey, Stats> = {} as Record<PeriodKey, Stats>;
    for (const [period, result] of results) {
      acc[period] = {
        highestTemperature: result._max.temperature ?? null,
        highestHumidity: result._max.humidity ?? null,
        averageTemperature: result._avg.temperature ?? null,
        averageHumidity: result._avg.humidity ?? null,
      };
    }
    return acc;
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

  async getUserOverview(userId: string) {
    const now = new Date();
    const lastHour = new Date(now.getTime() - 60 * 60 * 1000);
    const lastDay = new Date(now.getTime() - 24 * 60 * 60 * 1000);

    const devices = await this.prisma.device.findMany({
      where: { userId },
    });

    const activeDevices = devices.filter((d) => d.status === 'ACTIVE').length;
    const inactiveDevices = devices.filter(
      (d) => d.status === 'INACTIVE',
    ).length;
    const totalDevices = devices.length;

    const allTimeReadings = await this.prisma.reading.count({
      where: { device: { userId } },
    });

    const readingsLastHour = await this.prisma.reading.count({
      where: {
        device: { userId },
        readingTime: {
          gte: lastHour,
        },
      },
    });

    const readingsLastDay = await this.prisma.reading.count({
      where: {
        device: { userId },
        readingTime: {
          gte: lastDay,
        },
      },
    });

    const readingsPerMesh = await this.prisma.device.groupBy({
      by: ['meshId'],
      where: { userId },
      _count: {
        _all: true,
      },
    });

    const meshes = await this.prisma.mesh.findMany({
      where: { userId },
      select: {
        id: true,
        name: true,
      },
    });

    const readingsPerMeshWithName = readingsPerMesh.map((item) => {
      const mesh = meshes.find((m) => m.id === item.meshId);
      return {
        meshId: item.meshId,
        meshName: mesh ? mesh.name : 'Unknown',
        count: item._count._all,
      };
    });

    return {
      devices: {
        active: activeDevices,
        inactive: inactiveDevices,
        total: totalDevices,
      },
      readings: {
        allTime: allTimeReadings,
        lastHour: readingsLastHour,
        lastDay: readingsLastDay,
      },
      readingsPerMesh: readingsPerMeshWithName,
    };
  }
}
