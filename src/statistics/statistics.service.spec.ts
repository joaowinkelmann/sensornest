import { Test, TestingModule } from '@nestjs/testing';
import { StatisticsService } from './statistics.service';
import { PrismaService } from '../../prisma/prisma.service';
import { NotFoundException } from '@nestjs/common';

describe('StatisticsService', () => {
  let service: StatisticsService;
  let prisma: PrismaService;

  beforeEach(async () => {
    const module: TestingModule = await Test.createTestingModule({
      providers: [
        StatisticsService,
        {
          provide: PrismaService,
          useValue: {
            device: { findUnique: jest.fn(), findMany: jest.fn(), groupBy: jest.fn() },
            zone: { findUnique: jest.fn() },
            mesh: { findUnique: jest.fn(), findMany: jest.fn() },
            reading: {
              aggregate: jest.fn(),
              count: jest.fn(),
              groupBy: jest.fn(),
              findMany: jest.fn(),
            },
          },
        },
      ],
    }).compile();

    service = module.get<StatisticsService>(StatisticsService);
    prisma = module.get<PrismaService>(PrismaService);
  });

  it('should be defined', () => {
    expect(service).toBeDefined();
  });

  describe('getDeviceStatistics', () => {
    it('should throw NotFoundException if device not found', async () => {
      jest.spyOn(prisma.device, 'findUnique').mockResolvedValue(null);
      await expect(service.getDeviceStatistics('1')).rejects.toThrow(NotFoundException);
    });

    it('should return device statistics', async () => {
      jest.spyOn(prisma.device, 'findUnique').mockResolvedValue({ id: '1' } as any);
      jest.spyOn(prisma.reading, 'aggregate').mockResolvedValue({ _max: { temperature: 30 }, _avg: { temperature: 25 } } as any);
      const result = await service.getDeviceStatistics('1');
      expect(result).toBeDefined();
    });
  });

  describe('getZoneStatistics', () => {
    it('should throw NotFoundException if zone not found', async () => {
      jest.spyOn(prisma.zone, 'findUnique').mockResolvedValue(null);
      await expect(service.getZoneStatistics('1')).rejects.toThrow(NotFoundException);
    });

    it('should return zone statistics', async () => {
      jest.spyOn(prisma.zone, 'findUnique').mockResolvedValue({ id: '1' } as any);
      jest.spyOn(prisma.reading, 'aggregate').mockResolvedValue({ _max: { temperature: 30 }, _avg: { temperature: 25 } } as any);
      const result = await service.getZoneStatistics('1');
      expect(result).toBeDefined();
    });
  });

  describe('getMeshStatistics', () => {
    it('should throw NotFoundException if mesh not found', async () => {
      jest.spyOn(prisma.mesh, 'findUnique').mockResolvedValue(null);
      await expect(service.getMeshStatistics('1')).rejects.toThrow(NotFoundException);
    });

    it('should return mesh statistics', async () => {
      jest.spyOn(prisma.mesh, 'findUnique').mockResolvedValue({ id: '1' } as any);
      jest.spyOn(prisma.reading, 'aggregate').mockResolvedValue({ _max: { temperature: 30 }, _avg: { temperature: 25 } } as any);
      const result = await service.getMeshStatistics('1');
      expect(result).toBeDefined();
    });
  });

  describe('getUserOverview', () => {
    it('should return user overview statistics', async () => {
      const userId = 'user-1';
      const mockDevices = [
        { id: 'device-1', status: 'ACTIVE', userId, meshId: 'mesh-1' },
        { id: 'device-2', status: 'INACTIVE', userId, meshId: 'mesh-1' },
        { id: 'device-3', status: 'ACTIVE', userId, meshId: 'mesh-2' },
      ];
      const mockMeshes = [
        { id: 'mesh-1', name: 'Mesh 1' },
        { id: 'mesh-2', name: 'Mesh 2' },
      ];
      const mockReadingsCountAllTime = 100;
      const mockReadingsCountLastHour = 10;
      const mockReadingsCountLastDay = 50;

      jest.spyOn(prisma.device, 'findMany').mockResolvedValue(mockDevices as any);
      jest.spyOn(prisma.mesh, 'findMany').mockResolvedValue(mockMeshes as any);
      jest.spyOn(prisma.reading, 'count')
        .mockResolvedValueOnce(mockReadingsCountAllTime)
        .mockResolvedValueOnce(mockReadingsCountLastHour)
        .mockResolvedValueOnce(mockReadingsCountLastDay);
      jest.spyOn(prisma.device, 'groupBy').mockResolvedValue([
        { meshId: 'mesh-1', _count: { _all: 2 } },
        { meshId: 'mesh-2', _count: { _all: 1 } },
      ] as any);

      const result = await service.getUserOverview(userId);

      expect(result).toEqual({
        devices: {
          active: 2,
          inactive: 1,
          total: 3,
        },
        readings: {
          allTime: mockReadingsCountAllTime,
          lastHour: mockReadingsCountLastHour,
          lastDay: mockReadingsCountLastDay,
        },
        readingsPerMesh: [
          { meshId: 'mesh-1', meshName: 'Mesh 1', count: 2 },
          { meshId: 'mesh-2', meshName: 'Mesh 2', count: 1 },
        ],
      });
    });
  });

  describe('getDeviceGraph', () => {
    it('should return graphed statistics grouped by hour/day', async () => {
      const deviceId = 'device-1';
      const now = new Date();
      const readings = [
        // Last hour (for 'day')
        { deviceId, readingTime: new Date(now.getTime() - 1 * 60 * 60 * 1000), temperature: 20, humidity: 40 },
        // Last 2 hours (for 'day')
        { deviceId, readingTime: new Date(now.getTime() - 2 * 60 * 60 * 1000), temperature: 22, humidity: 42 },
        // Last 3 days (for 'week' and 'month')
        { deviceId, readingTime: new Date(now.getTime() - 3 * 24 * 60 * 60 * 1000), temperature: 18, humidity: 38 },
        // Last 20 days (for 'month')
        { deviceId, readingTime: new Date(now.getTime() - 20 * 24 * 60 * 60 * 1000), temperature: 25, humidity: 50 },
      ];
      jest.spyOn(prisma.reading, 'findMany').mockResolvedValue(readings as any);

      const result = await service.getDeviceGraph(deviceId);

      expect(Array.isArray(result.day)).toBe(true);
      expect(Array.isArray(result.week)).toBe(true);
      expect(Array.isArray(result.month)).toBe(true);

      // Each array should have at least one bucket
      expect(result.day.length).toBeGreaterThan(0);
      expect(result.week.length).toBeGreaterThan(0);
      expect(result.month.length).toBeGreaterThan(0);

      // All buckets should have avgTemperature and avgHumidity
      result.day.forEach((bucket) => {
        expect(bucket).toHaveProperty('avgTemperature');
        expect(bucket).toHaveProperty('avgHumidity');
      });
      result.week.forEach((bucket) => {
        expect(bucket).toHaveProperty('avgTemperature');
        expect(bucket).toHaveProperty('avgHumidity');
      });
      result.month.forEach((bucket) => {
        expect(bucket).toHaveProperty('avgTemperature');
        expect(bucket).toHaveProperty('avgHumidity');
      });
    });
  });
});
