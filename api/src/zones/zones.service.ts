import { Injectable } from '@nestjs/common';
import { PrismaService } from '../../prisma/prisma.service';
import { CreateZoneDto } from './dto/create-zone.dto';
import { UpdateZoneDto } from './dto/update-zone.dto';

@Injectable()
export class ZonesService {
  constructor(private readonly prisma: PrismaService) {}

  create(createZoneDto: CreateZoneDto) {
    return this.prisma.zone.create({
      data: createZoneDto,
    });
  }

  findAll() {
    return this.prisma.zone.findMany();
  }

  findOne(id: string) {
    return this.prisma.zone.findUnique({
      where: { id },
    });
  }

  // Pega as zones da mesh desejada e garante que a mesh pertença ao usuário
  findZonesByMesh(meshId: string, userId: string) {
    return this.prisma.zone.findMany({
      where: {
        meshId,
        mesh: { userId },
      },
    });
  }

  update(id: string, updateZoneDto: UpdateZoneDto) {
    return this.prisma.zone.update({
      where: { id },
      data: updateZoneDto,
    });
  }

  remove(id: string) {
    return this.prisma.zone.delete({
      where: { id },
    });
  }
}
