import { Module } from '@nestjs/common';
import { DataExporterController } from './data-exporter.controller';
import { DataExporterService } from './data-exporter.service';
import { PrismaModule } from '../../prisma/prisma.module';
import { UploaderModule } from '../uploader/uploader.module';

@Module({
  imports: [PrismaModule, UploaderModule],
  controllers: [DataExporterController],
  providers: [DataExporterService],
})
export class DataExporterModule {}
