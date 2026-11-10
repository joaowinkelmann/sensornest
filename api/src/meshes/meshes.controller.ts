import {
  Controller,
  Get,
  Post,
  Body,
  Patch,
  Param,
  Delete,
  Request,
  UseGuards,
  BadRequestException,
  Logger,
} from '@nestjs/common';
import * as fastify from 'fastify';
import { MultipartFile } from '@fastify/multipart';
import { MeshesService } from './meshes.service';
import { CreateMeshDto } from './dto/create-mesh.dto';
import { UpdateMeshDto } from './dto/update-mesh.dto';
import { AuthGuard } from '../auth/auth.guard';
import { ZonesService } from '../zones/zones.service';
import { ReqReturnDto } from '../auth/dto/req-return.dto';
import { DevicesService } from '../devices/devices.service';
import {
  ApiBearerAuth,
  ApiTags,
  ApiOperation,
  ApiResponse,
  ApiBody,
  ApiParam,
  ApiConsumes,
} from '@nestjs/swagger';
import { Mesh } from './entities/mesh.entity';
import { Zone } from '../zones/entities/zone.entity';

@UseGuards(AuthGuard)
@ApiBearerAuth()
@ApiTags('Meshes')
@Controller('meshes')
export class MeshesController {
  private readonly logger = new Logger(MeshesController.name);
  constructor(
    private readonly meshesService: MeshesService,
    private readonly zonesService: ZonesService,
    private readonly devicesService: DevicesService,
  ) {}

  @Post()
  @ApiOperation({ summary: 'Cria uma nova Mesh para o usuário autenticado.' })
  @ApiBody({ type: CreateMeshDto })
  @ApiResponse({
    status: 201,
    description: 'Mesh criada com sucesso.',
    type: Mesh,
  })
  create(@Body() createMeshDto: CreateMeshDto, @Request() req: ReqReturnDto) {
    return this.meshesService.create(createMeshDto, req.user.id);
  }

  @Get()
  @ApiOperation({ summary: 'Lista todas as meshes do usuário autenticado.' })
  @ApiResponse({
    status: 200,
    description: 'Lista de meshes.',
    type: [Mesh],
  })
  findAll(@Request() req: ReqReturnDto) {
    return this.meshesService.findAll(req.user.id);
  }

  @Get(':id')
  @ApiOperation({ summary: 'Busca uma mesh específica do usuário.' })
  @ApiParam({ name: 'id', description: 'ID da mesh.' })
  @ApiResponse({
    status: 200,
    description: 'Mesh encontrada.',
    type: Mesh,
  })
  @ApiResponse({ status: 404, description: 'Mesh não encontrada.' })
  findOne(@Param('id') id: string, @Request() req: ReqReturnDto) {
    return this.meshesService.findOne(id, req.user.id);
  }

  @Patch(':id')
  @ApiOperation({ summary: 'Atualiza uma mesh do usuário.' })
  @ApiParam({ name: 'id', description: 'ID da mesh.' })
  @ApiBody({ type: UpdateMeshDto })
  @ApiResponse({
    status: 200,
    description: 'Mesh atualizada.',
    type: Mesh,
  })
  @ApiResponse({ status: 404, description: 'Mesh não encontrada.' })
  update(
    @Param('id') id: string,
    @Body() updateMeshDto: UpdateMeshDto,
    @Request() req: ReqReturnDto,
  ) {
    return this.meshesService.update(id, updateMeshDto, req.user.id);
  }

  @Delete(':id')
  @ApiOperation({ summary: 'Remove uma mesh do usuário.' })
  @ApiParam({ name: 'id', description: 'ID da mesh.' })
  @ApiResponse({
    status: 200,
    description: 'Mesh removida.',
    type: Mesh,
  })
  @ApiResponse({ status: 404, description: 'Mesh não encontrada.' })
  remove(@Param('id') id: string, @Request() req: ReqReturnDto) {
    return this.meshesService.remove(id, req.user.id);
  }

  @Get(':meshId/zones')
  @ApiOperation({
    summary: 'Lista as zones pertencentes a uma mesh do usuário.',
  })
  @ApiParam({ name: 'meshId', description: 'ID da mesh.' })
  @ApiResponse({
    status: 200,
    description: 'Lista de zones da mesh.',
    type: [Zone],
  })
  findZonesByMesh(
    @Param('meshId') meshId: string,
    @Request() req: ReqReturnDto,
  ) {
    return this.zonesService.findZonesByMesh(meshId, req.user.id);
  }

  @ApiOperation({
    summary: 'Busca todos os dispositivos vinculados a uma mesh específica.',
  })
  @Get(':meshId/devices')
  @UseGuards(AuthGuard)
  findDevicesByZone(
    @Param('meshId') meshId: string,
    @Request() req: ReqReturnDto,
  ) {
    return this.devicesService.findDevicesByMesh(meshId, req.user.id);
  }

  @Post('upload-map')
  @ApiConsumes('multipart/form-data')
  @ApiOperation({
    summary:
      'Faz upload de uma imagem de mapa, gera quadrantes (z/x/y.webp) e envia ao OCI Object Storage. Retorna também o nível mínimo e máximo de zoom suportados.',
  })
  @ApiBody({
    schema: {
      type: 'object',
      properties: {
        file: { type: 'string', format: 'binary' },
        meshId: { type: 'string', nullable: false },
      },
    },
  })
  async uploadMap(
    @Request() req: fastify.FastifyRequest & { user: ReqReturnDto['user'] },
  ) {
    if (!req.isMultipart()) {
      this.logger.error('Request is not multipart');
      throw new BadRequestException('Request is not multipart');
    }

    let meshId: string | undefined;
    let file: (MultipartFile & { buffer: Buffer }) | undefined;

    this.logger.log('Processing multipart request for map upload...');
    const parts = req.parts();
    for await (const part of parts) {
      this.logger.debug(
        `Part received: type=${part.type}, fieldname=${part.fieldname}`,
      );
      if (part.type === 'field' && part.fieldname === 'meshId') {
        meshId = part.value as string;
        this.logger.log(`Found meshId: ${meshId}`);
      } else if (part.type === 'file') {
        this.logger.log(`Found file: ${part.filename}`);
        const buffer = await part.toBuffer();
        file = {
          ...part,
          buffer,
        };
      }
    }

    if (!file) {
      this.logger.error('File part was not found in the request.');
      throw new BadRequestException('No file uploaded');
    }

    if (!meshId) {
      this.logger.error('meshId field was not found in the request.');
      throw new BadRequestException('meshId is required');
    }

    return this.meshesService.uploadMap(file, req.user.id, meshId);
  }
}
