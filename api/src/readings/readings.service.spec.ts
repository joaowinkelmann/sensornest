import { Test, TestingModule } from '@nestjs/testing';
import { ReadingsService } from './readings.service';
import { PrismaService } from '../../prisma/prisma.service';

describe('ReadingsService', () => {
  let service: ReadingsService;

  beforeEach(async () => {
    const module: TestingModule = await Test.createTestingModule({
      providers: [ReadingsService, PrismaService],
    }).compile();

    service = module.get<ReadingsService>(ReadingsService);
  });

  it('should be defined', () => {
    expect(service).toBeDefined();
  });
});
