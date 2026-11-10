import { Injectable, BadRequestException, Logger } from '@nestjs/common';
import { PrismaService } from '../../prisma/prisma.service';
import { CreateMeshDto } from './dto/create-mesh.dto';
import { UpdateMeshDto } from './dto/update-mesh.dto';
import { TilerService } from '../tiler/tiler.service';
import { UploaderService } from '../uploader/uploader.service';
import * as fs from 'node:fs/promises';
import { MultipartFile } from '@fastify/multipart';

@Injectable()
export class MeshesService {
  private readonly logger = new Logger(MeshesService.name);
  constructor(
    private prisma: PrismaService,
    private tiler: TilerService,
    private uploader: UploaderService,
  ) { }

  async create(createMeshDto: CreateMeshDto, userId: string) {
    return await this.prisma.mesh.create({
      data: {
        ...createMeshDto,
        userId,
        name: createMeshDto.name,
      },
    });
  }

  async findAll(userId: string) {
    return await this.prisma.mesh.findMany({
      where: { userId },
    });
  }

  async findOne(id: string, userId: string) {
    return await this.prisma.mesh.findFirst({
      where: { id, userId },
    });
  }

  async update(id: string, updateMeshDto: UpdateMeshDto, userId: string) {
    const mesh = await this.findOne(id, userId);
    if (!mesh) return null;
    return await this.prisma.mesh.update({
      where: { id },
      data: updateMeshDto,
    });
  }

  async remove(id: string, userId: string) {
    const mesh = await this.findOne(id, userId);
    if (!mesh) return null;
    return await this.prisma.mesh.delete({
      where: { id },
    });
  }

  // Gera os tiles (ladrilhos) e faz upload para o OCI. Retorna o template de URL do Leaflet.
  async uploadMap(
    file: MultipartFile & { buffer: Buffer },
    userId: string,
    meshId: string,
  ) {
    if (!file) {
      this.logger.warn('uploadMap: Nenhum arquivo fornecido');
      throw new BadRequestException('Nenhum arquivo fornecido');
    }
    if (!meshId) {
      this.logger.warn('uploadMap: Nenhum meshId fornecido');
      throw new BadRequestException('Nenhum meshId fornecido');
    }

    // Suporta armazenamento em memória e disco
    const buffer = file.buffer;

    if (!buffer) {
      this.logger.warn('uploadMap: buffer de arquivo vazio');
      throw new BadRequestException('Buffer de arquivo vazio');
    } else {
      this.logger.debug(
        `uploadMap: Uploaded file size: ${buffer.length} bytes`,
      );
    }

    const { tmpDir, maxZoom, prefix, ext } =
      await this.tiler.makeTilesFromImageBuffer(buffer, meshId);

    try {
      await this.uploader.uploadDirectory(prefix, tmpDir);
    } finally {
      await fs
        .rm(tmpDir, { recursive: true, force: true })
        .catch(() => undefined);
    }

    const base = this.uploader.getBaseUrl();
    const mapUrl = `${base}/${prefix}/{z}/{x}/{y}.${ext}`;

    this.logger.log(`Mounted map URL: ${mapUrl}`);

    if (meshId && userId) {
      const mesh = await this.findOne(meshId, userId);
      if (mesh) {

        await this.prisma.mesh.update({
          where: { id: meshId },
          data: { mapUrl, mapMinZoom: 0, mapMaxZoom: maxZoom },
        });
      } else {
        this.logger.warn(
          `Mesh ${meshId} not found for user ${userId}; not updating mapUrl`,
        );
      }
    }

    return {
      mapUrl,
      maxZoom,
      minZoom: 0,
      prefix,
      ext,
    };
  }
}
