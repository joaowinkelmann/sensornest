import { Module } from '@nestjs/common';
import { AppController } from './app.controller';
import { AppService } from './app.service';
import { ReadingsModule } from './readings/readings.module';
import { PrismaService } from '../prisma/prisma.service';
import { AuthModule } from './auth/auth.module';
import { AuthGuard } from './auth/auth.guard';
import { UsersModule } from './users/users.module';
import { HealthModule } from './health/health.module';
import { MeshesModule } from './meshes/meshes.module';
import { ZonesModule } from './zones/zones.module';
import { DevicesModule } from './devices/devices.module';
import { UploaderModule } from './uploader/uploader.module';
import { TilerService } from './tiler/tiler.service';
import { ConfigModule, ConfigService } from '@nestjs/config';
import { MailerModule } from '@nestjs-modules/mailer';
import { ScheduleModule } from '@nestjs/schedule';
import { DeviceCheckerModule } from './device-checker/device-checker.module';
import { MailerServiceModule } from './mailer/mailer.module';
import { StatisticsModule } from './statistics/statistics.module';
import { DataExporterModule } from './data-exporter/data-exporter.module';

@Module({
  imports: [
    ScheduleModule.forRoot(),
    ReadingsModule,
    AuthModule,
    UsersModule,
    HealthModule,
    MeshesModule,
    ZonesModule,
    DevicesModule,
    UploaderModule,
    ConfigModule.forRoot({ envFilePath: '.env', isGlobal: true }),
    MailerModule.forRootAsync({
      imports: [ConfigModule],
      inject: [ConfigService],
      useFactory: (config: ConfigService) => ({
        transport: {
          host: config.get<string>('OCI_EMAIL_HOSTNAME'),
          port: 587,
          auth: {
            user: config.get<string>('OCI_EMAIL_USERNAME'),
            pass: config.get<string>('OCI_EMAIL_PASSWORD'),
          },
        },
        defaults: {
          from: '"No Reply" <noreply@winkels.com.br>',
        },
      }),
    }),
    DeviceCheckerModule,
    MailerServiceModule,
    StatisticsModule,
    DataExporterModule,
  ],
  controllers: [AppController],
  providers: [
    AppService,
    PrismaService,
    {
      provide: 'APP_GUARD',
      useClass: AuthGuard,
    },
    TilerService,
  ],
  exports: [PrismaService],
})
export class AppModule {}
