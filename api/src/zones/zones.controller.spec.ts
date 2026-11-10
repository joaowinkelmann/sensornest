import { Test, TestingModule } from '@nestjs/testing';
import { ZonesController } from './zones.controller';
import { ZonesService } from './zones.service';
import { PrismaService } from '../../prisma/prisma.service';
import { DevicesService } from '../devices/devices.service';

describe('ZonesController', () => {
  let controller: ZonesController;

  beforeEach(async () => {
    const module: TestingModule = await Test.createTestingModule({
      controllers: [ZonesController],
      providers: [
        ZonesService,
        { provide: PrismaService, useValue: {} },
        { provide: DevicesService, useValue: {} },
      ],
    }).compile();

    controller = module.get<ZonesController>(ZonesController);
  });

  it('should be defined', () => {
    expect(controller).toBeDefined();
  });
});
