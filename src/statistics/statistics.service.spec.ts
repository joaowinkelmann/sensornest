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
            device: { findUnique: jest.fn() },
            zone: { findUnique: jest.fn() },
            mesh: { findUnique: jest.fn() },
            reading: { aggregate: jest.fn() },
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
});
