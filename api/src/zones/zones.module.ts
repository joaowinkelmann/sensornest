import { Module } from '@nestjs/common';
import { ZonesService } from './zones.service';
import { ZonesController } from './zones.controller';
import { PrismaService } from '../../prisma/prisma.service';
import { DevicesService } from '../devices/devices.service';

@Module({
  controllers: [ZonesController],
  providers: [ZonesService, PrismaService, DevicesService],
})
export class ZonesModule {}
