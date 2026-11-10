import { Test, TestingModule } from '@nestjs/testing';
import { DeviceCheckerService } from './device-checker.service';
import { PrismaService } from '../../prisma/prisma.service';
import { ConfigService } from '@nestjs/config';

describe('DeviceCheckerService', () => {
  let service: DeviceCheckerService;

  beforeEach(async () => {
    const module: TestingModule = await Test.createTestingModule({
      providers: [
        DeviceCheckerService,
        { provide: PrismaService, useValue: {} },
        { provide: ConfigService, useValue: { get: jest.fn() } },
      ],
    }).compile();

    service = module.get<DeviceCheckerService>(DeviceCheckerService);
  });

  it('should be defined', () => {
    expect(service).toBeDefined();
  });
});
