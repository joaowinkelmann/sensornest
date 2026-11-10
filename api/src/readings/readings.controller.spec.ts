import { Test, TestingModule } from '@nestjs/testing';
import { ReadingsController } from './readings.controller';
import { ReadingsService } from './readings.service';
import { PrismaService } from '../../prisma/prisma.service';

describe('ReadingsController', () => {
  let controller: ReadingsController;

  beforeEach(async () => {
    const module: TestingModule = await Test.createTestingModule({
      controllers: [ReadingsController],
      providers: [ReadingsService, { provide: PrismaService, useValue: {} }],
    }).compile();

    controller = module.get<ReadingsController>(ReadingsController);
  });

  it('should be defined', () => {
    expect(controller).toBeDefined();
  });
});
