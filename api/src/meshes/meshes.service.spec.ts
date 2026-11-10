import { Test, TestingModule } from '@nestjs/testing';
import { MeshesService } from './meshes.service';
import { PrismaService } from '../../prisma/prisma.service';
import { TilerService } from '../tiler/tiler.service';
import { UploaderService } from '../uploader/uploader.service';

describe('MeshesService', () => {
  let service: MeshesService;

  beforeEach(async () => {
    const module: TestingModule = await Test.createTestingModule({
      providers: [
        MeshesService,
        { provide: PrismaService, useValue: {} },
        { provide: TilerService, useValue: {} },
        { provide: UploaderService, useValue: {} },
      ],
    }).compile();

    service = module.get<MeshesService>(MeshesService);
  });

  it('should be defined', () => {
    expect(service).toBeDefined();
  });
});
