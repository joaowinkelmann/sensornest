import { Test, TestingModule } from '@nestjs/testing';
import { ZonesService } from './zones.service';
import { PrismaService } from '../../prisma/prisma.service';

describe('ZonesService', () => {
  let service: ZonesService;

  beforeEach(async () => {
    const module: TestingModule = await Test.createTestingModule({
      providers: [ZonesService, { provide: PrismaService, useValue: {} }],
    }).compile();

    service = module.get<ZonesService>(ZonesService);
  });

  it('should be defined', () => {
    expect(service).toBeDefined();
  });
});
