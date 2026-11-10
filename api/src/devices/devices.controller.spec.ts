import { Test, TestingModule } from '@nestjs/testing';
import { DevicesController } from './devices.controller';
import { DevicesService } from './devices.service';
import { PrismaService } from '../../prisma/prisma.service';

describe('DevicesController', () => {
  let controller: DevicesController;

  beforeEach(async () => {
    const module: TestingModule = await Test.createTestingModule({
      controllers: [DevicesController],
      providers: [DevicesService, { provide: PrismaService, useValue: {} }],
    }).compile();

    controller = module.get<DevicesController>(DevicesController);
  });

  it('should be defined', () => {
    expect(controller).toBeDefined();
  });
});
