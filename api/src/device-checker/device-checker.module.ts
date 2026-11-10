import { Module } from '@nestjs/common';
import { DeviceCheckerService } from './device-checker.service';
import { PrismaModule } from '../../prisma/prisma.module';

@Module({
  imports: [PrismaModule],
  providers: [DeviceCheckerService],
})
export class DeviceCheckerModule {}
