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
import { ExportResultDto } from './dto/export-result.dto';

@Injectable()
export class DataExporterService {
  private readonly logger = new Logger(DataExporterService.name);

  constructor(
    private prisma: PrismaService,
    private uploader: UploaderService,
  ) {}

  async exportData(
    dto: ExportDataDto,
    userId: string,
  ): Promise<ExportResultDto> {
    const {
      entityType,
      entityId,
      chunkSize = 100_000,
      pageSize = 10_000,
      from,
      to,
      includeHeadersPerFile = true,
    } = dto;

    this.logger.log(
      `Starting data export for ${entityType}:${entityId} (chunk=${chunkSize}, page=${pageSize})`,
    );

    // Validate ownership without loading readings
    await this.assertOwnership(entityType, entityId, userId);

    // Build reading where clause
    const timeFilter =
      from || to
        ? {
            readingTime: {
              ...(from ? { gte: new Date(from) } : {}),
              ...(to ? { lte: new Date(to) } : {}),
            },
          }
        : {};

    const baseWhere =
      entityType === 'device'
        ? { deviceId: entityId }
        : entityType === 'zone'
          ? { device: { zoneId: entityId } }
          : { device: { meshId: entityId } };

    // Iterate readings in pages
    let cursor: { id: string } | undefined;
    let rowsInCurrentChunk: any[] = [];
    let totalRows = 0;
    let partIndex = 0;
    const files: string[] = [];
    const startedAt = new Date().toISOString();

    const uploadChunk = async (rows: any[], index: number) => {
      if (rows.length === 0) return;
      const csv = Papa.unparse(rows, {
        header: includeHeadersPerFile || index === 0,
      });
      const csvBuffer = Buffer.from(csv, 'utf-8');
      const fileName = `export-${entityType}-${entityId}-${startedAt}-part-${String(
        index + 1,
      ).padStart(3, '0')}.csv`;
      this.logger.log(
        `Uploading CSV part ${index + 1}: ${fileName} (${csvBuffer.length} bytes)`,
      );
      const url = await this.uploader.uploadObject(
        fileName,
        csvBuffer,
        'text/csv',
      );
      files.push(url);
    };

    // Paged loop
    // Order by id asc to use cursor pagination
    for (;;) {
      const page = await this.prisma.reading.findMany({
        where: { ...baseWhere, ...timeFilter },
        include: {
          device: {
            select: {
              id: true,
              name: true,
              zone: { select: { id: true, name: true } },
              mesh: { select: { id: true, name: true } },
            },
          },
        },
        orderBy: { id: 'asc' },
        take: pageSize,
        ...(cursor ? { cursor, skip: 1 } : {}),
      });

      if (page.length === 0) break;

      for (const r of page) {
        // map to flat CSV row
        rowsInCurrentChunk.push({
          meshId: r.device?.mesh?.id ?? null,
          meshName: r.device?.mesh?.name ?? null,
          zoneId: r.device?.zone?.id ?? null,
          zoneName: r.device?.zone?.name ?? null,
          deviceId: r.device?.id ?? null,
          deviceName: r.device?.name ?? null,
          readingId: r.id,
          temperature: r.temperature,
          humidity: r.humidity,
          readingTime: r.readingTime.toISOString(),
        });

        totalRows++;

        if (rowsInCurrentChunk.length >= chunkSize) {
          await uploadChunk(rowsInCurrentChunk, partIndex);
          partIndex++;
          rowsInCurrentChunk = [];
        }
      }

      cursor = { id: page[page.length - 1].id };
    }

    // Flush remaining rows
    if (rowsInCurrentChunk.length > 0) {
      await uploadChunk(rowsInCurrentChunk, partIndex);
      partIndex++;
      rowsInCurrentChunk = [];
    }

    this.logger.log(
      `Export completed for ${entityType}:${entityId} -> ${files.length} file(s), ${totalRows} row(s).`,
    );

    return {
      entityType,
      entityId,
      totalRows,
      chunkSize,
      fileCount: files.length,
      files,
      from: from ?? null,
      to: to ?? null,
    };
  }

  // Validates entity exists and belongs to user (keeps existing behavior of throwing 404).
  private async assertOwnership(
    entityType: string,
    entityId: string,
    userId: string,
  ) {
    if (entityType === 'mesh') {
      const mesh = await this.prisma.mesh.findFirst({
        where: { id: entityId, userId },
        select: { id: true },
      });
      if (!mesh)
        throw new NotFoundException(`Mesh with ID ${entityId} not found.`);
      return;
    }
    if (entityType === 'zone') {
      const zone = await this.prisma.zone.findFirst({
        where: { id: entityId, mesh: { userId } },
        select: { id: true },
      });
      if (!zone)
        throw new NotFoundException(`Zone with ID ${entityId} not found.`);
      return;
    }
    if (entityType === 'device') {
      const device = await this.prisma.device.findFirst({
        where: { id: entityId, userId },
        select: { id: true },
      });
      if (!device)
        throw new NotFoundException(`Device with ID ${entityId} not found.`);
      return;
    }
    throw new BadRequestException('Invalid entity type');
  }
}
