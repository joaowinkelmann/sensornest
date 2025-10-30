import {
  Injectable,
  NotFoundException,
  BadRequestException,
  Logger,
} from '@nestjs/common';
import { PrismaService } from '../../prisma/prisma.service';
import { UploaderService } from '../uploader/uploader.service';
import { ExportDataDto } from './dto/export-data.dto';
import * as Papa from 'papaparse';

@Injectable()
export class DataExporterService {
  private readonly logger = new Logger(DataExporterService.name);

  constructor(
    private prisma: PrismaService,
    private uploader: UploaderService,
  ) {}

  async exportData(
    { entityType, entityId }: ExportDataDto,
    userId: string,
  ): Promise<string> {
    this.logger.log(
      `Starting data export for entity type: ${entityType}, ID: ${entityId}`,
    );

    const readings = await this.fetchReadings(entityType, entityId, userId);

    if (readings.length === 0) {
      this.logger.warn(
        `No readings found for ${entityType} ${entityId}, returning empty CSV URL.`,
      );
    }

    const csv = Papa.unparse(readings);
    const csvBuffer = Buffer.from(csv, 'utf-8');

    const fileName = `export-${entityType}-${entityId}-${new Date().toISOString()}.csv`;

    this.logger.log(`Uploading CSV file: ${fileName}`);
    const url = await this.uploader.uploadObject(
      fileName,
      csvBuffer,
      'text/csv',
    );
    this.logger.log(`CSV file uploaded to: ${url}`);
    return url;
  }

  private async fetchReadings(
    entityType: string,
    entityId: string,
    userId: string,
  ) {
    switch (entityType) {
      case 'mesh':
        return this.fetchReadingsForMesh(entityId, userId);
      case 'zone':
        return this.fetchReadingsForZone(entityId, userId);
      case 'device':
        return this.fetchReadingsForDevice(entityId, userId);
      default:
        throw new BadRequestException('Invalid entity type');
    }
  }

  private async fetchReadingsForMesh(meshId: string, userId: string) {
    const mesh = await this.prisma.mesh.findFirst({
      where: { id: meshId, userId },
      include: {
        devices: {
          include: {
            readings: true,
            zone: true,
          },
        },
      },
    });

    if (!mesh) {
      throw new NotFoundException(`Mesh with ID ${meshId} not found.`);
    }

    return mesh.devices.flatMap((device) =>
      device.readings.map((reading) => ({
        meshId: mesh.id,
        meshName: mesh.name,
        zoneId: device.zone?.id || null,
        zoneName: device.zone?.name || null,
        deviceId: device.id,
        deviceName: device.name,
        readingId: reading.id,
        temperature: reading.temperature,
        humidity: reading.humidity,
        readingTime: reading.readingTime.toISOString(),
      })),
    );
  }

  private async fetchReadingsForZone(zoneId: string, userId: string) {
    const zone = await this.prisma.zone.findFirst({
      where: { id: zoneId, mesh: { userId } },
      include: {
        mesh: true,
        devices: {
          include: {
            readings: true,
          },
        },
      },
    });

    if (!zone) {
      throw new NotFoundException(`Zone with ID ${zoneId} not found.`);
    }

    return zone.devices.flatMap((device) =>
      device.readings.map((reading) => ({
        meshId: zone.mesh.id,
        meshName: zone.mesh.name,
        zoneId: zone.id,
        zoneName: zone.name,
        deviceId: device.id,
        deviceName: device.name,
        readingId: reading.id,
        temperature: reading.temperature,
        humidity: reading.humidity,
        readingTime: reading.readingTime.toISOString(),
      })),
    );
  }

  private async fetchReadingsForDevice(deviceId: string, userId: string) {
    const device = await this.prisma.device.findFirst({
      where: { id: deviceId, userId },
      include: {
        mesh: true,
        zone: true,
        readings: true,
      },
    });

    if (!device) {
      throw new NotFoundException(`Device with ID ${deviceId} not found.`);
    }

    return device.readings.map((reading) => ({
      meshId: device.mesh.id,
      meshName: device.mesh.name,
      zoneId: device.zone?.id || null,
      zoneName: device.zone?.name || null,
      deviceId: device.id,
      deviceName: device.name,
      readingId: reading.id,
      temperature: reading.temperature,
      humidity: reading.humidity,
      readingTime: reading.readingTime.toISOString(),
    }));
  }
}
