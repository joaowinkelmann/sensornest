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
    // Added reading mock to support paged export
    reading: {
      findMany: jest.fn(),
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

    // Default empty array to avoid undefined `page.length` in tests that don't override
    mockPrismaService.reading.findMany.mockResolvedValue([]);
  });

  afterEach(() => {
    jest.clearAllMocks();
  });

  it('should be defined', () => {
    expect(service).toBeDefined();
  });

  describe('exportData', () => {
    it('should export data for a mesh (chunked)', async () => {
      // Ownership ok
      mockPrismaService.mesh.findFirst.mockResolvedValue({ id: 'mesh-id' });

      // Two pages: 2 rows then 1 row, then empty
      const now = new Date();
      const page1 = [
        {
          id: 'r-1',
          temperature: 25,
          humidity: 50,
          readingTime: now,
          device: {
            id: 'd-1',
            name: 'Device 1',
            zone: { id: 'zone-1', name: 'Zone 1' },
            mesh: { id: 'mesh-id', name: 'Mesh 1' },
          },
        },
        {
          id: 'r-2',
          temperature: 26,
          humidity: 51,
          readingTime: now,
          device: {
            id: 'd-2',
            name: 'Device 2',
            zone: { id: 'zone-2', name: 'Zone 2' },
            mesh: { id: 'mesh-id', name: 'Mesh 1' },
          },
        },
      ];
      const page2 = [
        {
          id: 'r-3',
          temperature: 27,
          humidity: 52,
          readingTime: now,
          device: {
            id: 'd-3',
            name: 'Device 3',
            zone: { id: 'zone-3', name: 'Zone 3' },
            mesh: { id: 'mesh-id', name: 'Mesh 1' },
          },
        },
      ];

      mockPrismaService.reading.findMany
        .mockResolvedValueOnce(page1)
        .mockResolvedValueOnce(page2)
        .mockResolvedValueOnce([]);

      (Papa.unparse as jest.Mock).mockReturnValue('csv-content');

      const result = await service.exportData(
        // chunkSize=2 forces 2 uploads for 3 rows
        { entityType: 'mesh', entityId: 'mesh-id', chunkSize: 2, pageSize: 2, includeHeadersPerFile: true },
        'user-id',
      );

      expect(prisma.mesh.findFirst).toHaveBeenCalledWith({
        where: { id: 'mesh-id', userId: 'user-id' },
        select: { id: true },
      });
      expect(prisma.reading.findMany).toHaveBeenCalled();
      expect(Papa.unparse).toHaveBeenCalledTimes(2);
      expect(uploader.uploadObject).toHaveBeenCalledTimes(2);

      expect(result.totalRows).toBe(3);
      expect(result.fileCount).toBe(2);
      expect(Array.isArray(result.files)).toBe(true);
      expect(result.files.length).toBe(2);
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
      // Ensure ownership check passes
      mockPrismaService.zone.findFirst.mockResolvedValue({ id: 'zone-id' });

      const now = new Date();
      const page1 = [
        {
          id: 'r-1',
          temperature: 20,
          humidity: 40,
          readingTime: now,
          device: {
            id: 'd-1',
            name: 'Device 1',
            zone: { id: 'zone-id', name: 'Zone Name' },
            mesh: { id: 'mesh-1', name: 'Mesh Name' },
          },
        },
      ];

      // Paged results: one row then end
      mockPrismaService.reading.findMany
        .mockResolvedValueOnce(page1)
        .mockResolvedValueOnce([]);

      (Papa.unparse as jest.Mock).mockReturnValue('csv-content');

      const result = await service.exportData(
        { entityType: 'zone', entityId: 'zone-id', chunkSize: 1000, pageSize: 1000 },
        'user-id',
      );

      expect(prisma.zone.findFirst).toHaveBeenCalledWith({
        where: { id: 'zone-id', mesh: { userId: 'user-id' } },
        select: { id: true },
      });
      expect(prisma.reading.findMany).toHaveBeenCalled();
      expect(Papa.unparse).toHaveBeenCalledTimes(1);
      expect(uploader.uploadObject).toHaveBeenCalledTimes(1);

      expect(result.totalRows).toBe(1);
      expect(result.fileCount).toBe(1);
      expect(result.files.length).toBe(1);
    });

    it('should export data for a device (chunked)', async () => {
      mockPrismaService.device.findFirst.mockResolvedValue({ id: 'device-id' });

      const now = new Date();
      const page1 = [
        {
          id: 'r-1',
          temperature: 30,
          humidity: 60,
          readingTime: now,
          device: {
            id: 'device-id',
            name: 'Device Name',
            zone: { id: 'zone-1', name: 'Zone 1' },
            mesh: { id: 'mesh-1', name: 'Mesh 1' },
          },
        },
      ];

      mockPrismaService.reading.findMany
        .mockResolvedValueOnce(page1)
        .mockResolvedValueOnce([]);

      (Papa.unparse as jest.Mock).mockReturnValue('csv-content');

      const result = await service.exportData(
        { entityType: 'device', entityId: 'device-id', chunkSize: 1000, pageSize: 1000 },
        'user-id',
      );

      expect(prisma.device.findFirst).toHaveBeenCalledWith({
        where: { id: 'device-id', userId: 'user-id' },
        select: { id: true },
      });
      expect(prisma.reading.findMany).toHaveBeenCalled();
      expect(Papa.unparse).toHaveBeenCalledTimes(1);
      expect(uploader.uploadObject).toHaveBeenCalledTimes(1);

      expect(result.totalRows).toBe(1);
      expect(result.fileCount).toBe(1);
      expect(result.files.length).toBe(1);
    });
  });
});
