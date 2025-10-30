import { Test, TestingModule } from '@nestjs/testing';
import { DataExporterService } from './data-exporter.service';
import { PrismaService } from '../../prisma/prisma.service';
import { UploaderService } from '../uploader/uploader.service';
import { NotFoundException } from '@nestjs/common';
import * as Papa from 'papaparse';

jest.mock('papaparse', () => ({
  unparse: jest.fn(),
}));

describe('DataExporterService', () => {
  let service: DataExporterService;
  let prisma: PrismaService;
  let uploader: UploaderService;

  const mockPrismaService = {
    mesh: {
      findFirst: jest.fn(),
    },
    zone: {
      findFirst: jest.fn(),
    },
    device: {
      findFirst: jest.fn(),
    },
  };

  const mockUploaderService = {
    uploadObject: jest.fn().mockResolvedValue('http://uploaded-file-url.com'),
  };

  beforeEach(async () => {
    const module: TestingModule = await Test.createTestingModule({
      providers: [
        DataExporterService,
        { provide: PrismaService, useValue: mockPrismaService },
        { provide: UploaderService, useValue: mockUploaderService },
      ],
    }).compile();

    service = module.get<DataExporterService>(DataExporterService);
    prisma = module.get<PrismaService>(PrismaService);
    uploader = module.get<UploaderService>(UploaderService);
  });

  afterEach(() => {
    jest.clearAllMocks();
  });

  it('should be defined', () => {
    expect(service).toBeDefined();
  });

  describe('exportData', () => {
    it('should export data for a mesh', async () => {
      const meshData = {
        id: 'mesh-id',
        name: 'Test Mesh',
        devices: [
          {
            id: 'device-id',
            name: 'Test Device',
            zone: { id: 'zone-id', name: 'Test Zone' },
            readings: [
              {
                id: 'reading-id',
                temperature: 25,
                humidity: 50,
                readingTime: new Date(),
              },
            ],
          },
        ],
      };
      mockPrismaService.mesh.findFirst.mockResolvedValue(meshData);
      (Papa.unparse as jest.Mock).mockReturnValue('csv-content');

      const url = await service.exportData(
        { entityType: 'mesh', entityId: 'mesh-id' },
        'user-id',
      );

      expect(prisma.mesh.findFirst).toHaveBeenCalledWith({
        where: { id: 'mesh-id', userId: 'user-id' },
        include: {
          devices: {
            include: {
              readings: true,
              zone: true,
            },
          },
        },
      });
      expect(Papa.unparse).toHaveBeenCalled();
      expect(uploader.uploadObject).toHaveBeenCalledWith(
        expect.any(String),
        expect.any(Buffer),
        'text/csv',
      );
      expect(url).toBe('http://uploaded-file-url.com');
    });

    it('should throw NotFoundException if mesh not found', async () => {
      mockPrismaService.mesh.findFirst.mockResolvedValue(null);
      await expect(
        service.exportData(
          { entityType: 'mesh', entityId: 'mesh-id' },
          'user-id',
        ),
      ).rejects.toThrow(NotFoundException);
    });

    it('should export data for a zone', async () => {
        const zoneData = {
          id: 'zone-id',
          name: 'Test Zone',
          mesh: { id: 'mesh-id', name: 'Test Mesh' },
          devices: [
            {
              id: 'device-id',
              name: 'Test Device',
              readings: [
                {
                  id: 'reading-id',
                  temperature: 25,
                  humidity: 50,
                  readingTime: new Date(),
                },
              ],
            },
          ],
        };
        mockPrismaService.zone.findFirst.mockResolvedValue(zoneData);
        (Papa.unparse as jest.Mock).mockReturnValue('csv-content');

        const url = await service.exportData(
          { entityType: 'zone', entityId: 'zone-id' },
          'user-id',
        );

        expect(prisma.zone.findFirst).toHaveBeenCalledWith({
          where: { id: 'zone-id', mesh: { userId: 'user-id' } },
          include: {
            mesh: true,
            devices: {
              include: {
                readings: true,
              },
            },
          },
        });
        expect(Papa.unparse).toHaveBeenCalled();
        expect(uploader.uploadObject).toHaveBeenCalledWith(
          expect.any(String),
          expect.any(Buffer),
          'text/csv',
        );
        expect(url).toBe('http://uploaded-file-url.com');
      });

      it('should export data for a device', async () => {
        const deviceData = {
          id: 'device-id',
          name: 'Test Device',
          mesh: { id: 'mesh-id', name: 'Test Mesh' },
          zone: { id: 'zone-id', name: 'Test Zone' },
          readings: [
            {
              id: 'reading-id',
              temperature: 25,
              humidity: 50,
              readingTime: new Date(),
            },
          ],
        };
        mockPrismaService.device.findFirst.mockResolvedValue(deviceData);
        (Papa.unparse as jest.Mock).mockReturnValue('csv-content');

        const url = await service.exportData(
          { entityType: 'device', entityId: 'device-id' },
          'user-id',
        );

        expect(prisma.device.findFirst).toHaveBeenCalledWith({
          where: { id: 'device-id', userId: 'user-id' },
          include: {
            mesh: true,
            zone: true,
            readings: true,
          },
        });
        expect(Papa.unparse).toHaveBeenCalled();
        expect(uploader.uploadObject).toHaveBeenCalledWith(
          expect.any(String),
          expect.any(Buffer),
          'text/csv',
        );
        expect(url).toBe('http://uploaded-file-url.com');
      });
  });
});
