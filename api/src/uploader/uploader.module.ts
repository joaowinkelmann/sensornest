import { Module } from '@nestjs/common';
import { UploaderService } from './uploader.service';
import { TilerService } from '../tiler/tiler.service';

@Module({
  providers: [UploaderService, TilerService],
  exports: [UploaderService, TilerService],
})
export class UploaderModule {}
