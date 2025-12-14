import { Injectable, NotFoundException } from '@nestjs/common';
import { PrismaService } from '../../prisma/prisma.service';
import { subDays, subWeeks, subMonths, subYears } from 'date-fns';
import { GraphReturnDto } from './dto/graph-return.dto';
import { OverviewReturnDto } from './dto/overview-return.dto';
import { StatisticsReturnDto, Stats } from './dto/statistics-return.dto';

type PeriodKey = 'day' | 'week' | 'month' | 'year';

type ReadingAgg = {
  _max: { temperature: number | null; humidity: number | null };
  _avg: { temperature: number | null; humidity: number | null };
};

@Injectable()
export class StatisticsService {
  constructor(private readonly prisma: PrismaService) { }

  private async getStatistics(
    model: 'device' | 'zone' | 'mesh',
    id: string,
  ): Promise<StatisticsReturnDto> {
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

  async getDeviceStatistics(deviceId: string): Promise<StatisticsReturnDto> {
    const device = await this.prisma.device.findUnique({
      where: { id: deviceId },
    });
    if (!device) {
      throw new NotFoundException('Device not found');
    }
    return this.getStatistics('device', deviceId);
  }

  async getZoneStatistics(zoneId: string): Promise<StatisticsReturnDto> {
    const zone = await this.prisma.zone.findUnique({ where: { id: zoneId } });
    if (!zone) {
      throw new NotFoundException('Zone not found');
    }
    return this.getStatistics('zone', zoneId);
  }

  async getMeshStatistics(meshId: string): Promise<StatisticsReturnDto> {
    const mesh = await this.prisma.mesh.findUnique({ where: { id: meshId } });
    if (!mesh) {
      throw new NotFoundException('Mesh not found');
    }
    return this.getStatistics('mesh', meshId);
  }

  async getUserOverview(userId: string): Promise<OverviewReturnDto> {
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

  async getDeviceGraph(deviceId: string): Promise<GraphReturnDto> {
    // Auxiliar para agrupar leituras por hora/dia
    const groupByPeriod = async (
      period: 'hour' | 'day',
      since: Date,
    ): Promise<GraphReturnDto['day']> => {
      const readings = await this.prisma.reading.findMany({
        where: {
          deviceId,
          readingTime: { gte: since },
        },
        orderBy: { readingTime: 'asc' },
      });

      const buckets: Record<
        string,
        { temperature: number[]; humidity: number[] }
      > = {};

      readings.forEach((r) => {
        const date = new Date(r.readingTime);
        let key: string;
        if (period === 'hour') {
          key = date.toISOString().slice(0, 13); // YYYY-MM-DDTHH
        } else {
          key = date.toISOString().slice(0, 10); // YYYY-MM-DD
        }
        if (!buckets[key]) buckets[key] = { temperature: [], humidity: [] };
        if (r.temperature !== null && r.temperature !== undefined)
          buckets[key].temperature.push(r.temperature);
        if (r.humidity !== null && r.humidity !== undefined)
          buckets[key].humidity.push(r.humidity);
      });

      return Object.entries(buckets).map(([bucket, vals]) => ({
        bucket,
        avgTemperature:
          vals.temperature.length > 0
            ? vals.temperature.reduce((a, b) => a + b, 0) /
            vals.temperature.length
            : 0,
        avgHumidity:
          vals.humidity.length > 0
            ? vals.humidity.reduce((a, b) => a + b, 0) / vals.humidity.length
            : 0,
      }));
    };

    const now = new Date();
    const dayAgo = new Date(now.getTime() - 24 * 60 * 60 * 1000);
    const weekAgo = new Date(now.getTime() - 7 * 24 * 60 * 60 * 1000);
    const monthAgo = new Date(now.getTime() - 30 * 24 * 60 * 60 * 1000);

    return {
      day: await groupByPeriod('hour', dayAgo),
      week: await groupByPeriod('day', weekAgo),
      month: await groupByPeriod('day', monthAgo),
    };
  }
}
