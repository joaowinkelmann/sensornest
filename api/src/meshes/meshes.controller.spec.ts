import { Test, TestingModule } from '@nestjs/testing';
import { MeshesController } from './meshes.controller';
import { MeshesService } from './meshes.service';
import { ZonesService } from '../zones/zones.service';
import { DevicesService } from '../devices/devices.service';
import { PrismaService } from '../../prisma/prisma.service';
import { TilerService } from '../tiler/tiler.service';
import { UploaderService } from '../uploader/uploader.service';
import { AuthGuard } from '../auth/auth.guard';
import { Reflector } from '@nestjs/core';

describe('MeshesController', () => {
  let controller: MeshesController;

  beforeEach(async () => {
    const module: TestingModule = await Test.createTestingModule({
      controllers: [MeshesController],
      providers: [
        MeshesService,
        { provide: ZonesService, useValue: {} },
        { provide: DevicesService, useValue: {} },
        { provide: PrismaService, useValue: {} },
        {
          provide: TilerService,
          useValue: { makeTilesFromImageBuffer: jest.fn() },
        },
        {
          provide: UploaderService,
          useValue: { uploadDirectory: jest.fn(), getBaseUrl: jest.fn() },
        },
        {
          provide: AuthGuard,
          useValue: { canActivate: jest.fn(() => true) },
        },
        Reflector,
      ],
    }).compile();

    controller = module.get<MeshesController>(MeshesController);
  });

  it('should be defined', () => {
    expect(controller).toBeDefined();
  });
});
