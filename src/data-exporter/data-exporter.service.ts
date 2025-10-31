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
      maxBytesPerFile, // NEW: optional cap in bytes
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

    // NEW: determine byte cap (prefer DTO, then ENV, then default 15 MiB)
    const envMaxBytes = Number(process.env.EXPORT_MAX_BYTES);
    const byteCap =
      (Number.isFinite(envMaxBytes) ? envMaxBytes : undefined) ??
      maxBytesPerFile ??
      15 * 1024 * 1024; // 15 MiB

    // NEW: fixed CSV column order used for estimates
    const csvFields = [
      'meshId',
      'meshName',
      'zoneId',
      'zoneName',
      'deviceId',
      'deviceName',
      'readingId',
      'temperature',
      'humidity',
      'readingTime',
    ] as const;

    // NEW: estimate bytes for a single CSV cell (approximate, conservative)
    const estimateCsvCellBytes = (val: unknown) => {
      const s =
        val === null || val === undefined
          ? ''
          : typeof val === 'number'
            ? String(val)
            : String(val);
      const quoteCount = (s.match(/"/g) || []).length;
      const needsQuotes = /[",\n]/.test(s);
      // Double quotes within value + wrap quotes if needed
      return s.length + quoteCount + (needsQuotes ? 2 : 0);
    };

    // NEW: estimate bytes for the header row (if emitted)
    const headerBytes =
      csvFields.reduce(
        (acc, key, i) =>
          acc +
          estimateCsvCellBytes(String(key)) +
          (i < csvFields.length - 1 ? 1 : 0),
        0,
      ) + 1; // newline

    // NEW: current estimated bytes in the chunk (start with header if we’ll include per file)
    let currentChunkEstimatedBytes = includeHeadersPerFile ? headerBytes : 0;

    const estimateRowBytes = (row: Record<string, unknown>) =>
      csvFields.reduce(
        (acc, key, i) =>
          acc +
          estimateCsvCellBytes(row[key]) +
          (i < csvFields.length - 1 ? 1 : 0),
        0,
      ) + 1; // newline

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
        const flatRow = {
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
        };

        // NEW: split by byte cap before pushing the row (exact upload still via Papa for the chunk)
        const rowBytes = estimateRowBytes(flatRow);
        if (
          rowsInCurrentChunk.length > 0 &&
          currentChunkEstimatedBytes + rowBytes > byteCap
        ) {
          await uploadChunk(rowsInCurrentChunk, partIndex);
          partIndex++;
          rowsInCurrentChunk = [];
          currentChunkEstimatedBytes = includeHeadersPerFile ? headerBytes : 0;
        }

        rowsInCurrentChunk.push(flatRow);
        currentChunkEstimatedBytes += rowBytes;

        totalRows++;

        if (rowsInCurrentChunk.length >= chunkSize) {
          await uploadChunk(rowsInCurrentChunk, partIndex);
          partIndex++;
          rowsInCurrentChunk = [];
          currentChunkEstimatedBytes = includeHeadersPerFile ? headerBytes : 0;
        }
      }

      cursor = { id: page[page.length - 1].id };
    }

    // Flush remaining rows
    if (rowsInCurrentChunk.length > 0) {
      await uploadChunk(rowsInCurrentChunk, partIndex);
      partIndex++;
      rowsInCurrentChunk = [];
      currentChunkEstimatedBytes = includeHeadersPerFile ? headerBytes : 0;
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
