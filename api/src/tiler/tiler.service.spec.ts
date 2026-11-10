import { Test, TestingModule } from '@nestjs/testing';
import { TilerService } from './tiler.service';

describe('TilerService', () => {
  let service: TilerService;

  beforeEach(async () => {
    const module: TestingModule = await Test.createTestingModule({
      providers: [TilerService],
    }).compile();

    service = module.get<TilerService>(TilerService);
  });

  it('should be defined', () => {
    expect(service).toBeDefined();
  });
});
