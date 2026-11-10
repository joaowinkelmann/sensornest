import { Test, TestingModule } from '@nestjs/testing';
import { DataExporterController } from './data-exporter.controller';
import { DataExporterService } from './data-exporter.service';
import { AuthGuard } from '../auth/auth.guard';
import { JwtService } from '@nestjs/jwt';

describe('DataExporterController', () => {
  let controller: DataExporterController;
  let service: DataExporterService;

  const mockDataExporterService = {
    exportData: jest.fn().mockResolvedValue('http://exported-file-url.com'),
  };

  beforeEach(async () => {
    const module: TestingModule = await Test.createTestingModule({
      controllers: [DataExporterController],
      providers: [
        {
          provide: DataExporterService,
          useValue: mockDataExporterService,
        },
        {
          provide: AuthGuard,
          useValue: { canActivate: jest.fn().mockReturnValue(true) },
        },
        {
          provide: JwtService,
          useValue: {
            verify: jest.fn(),
          },
        },
      ],
    }).compile();

    controller = module.get<DataExporterController>(DataExporterController);
    service = module.get<DataExporterService>(DataExporterService);
  });

  it('should be defined', () => {
    expect(controller).toBeDefined();
  });

  describe('exportData', () => {
    it('should call the data exporter service and return a file URL', async () => {
      const exportDataDto = {
        entityType: 'mesh',
        entityId: 'test-mesh-id',
      };
      const req = { user: { id: 'test-user-id' } };
      const result = await controller.exportData(exportDataDto, req as any);

      expect(service.exportData).toHaveBeenCalledWith(
        exportDataDto,
        'test-user-id',
      );
      expect(result).toBe('http://exported-file-url.com');
    });
  });
});
