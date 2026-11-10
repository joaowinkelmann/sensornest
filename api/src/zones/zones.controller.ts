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
  HttpCode,
  HttpStatus,
} from '@nestjs/common';
import { ZonesService } from './zones.service';
import { CreateZoneDto } from './dto/create-zone.dto';
import { UpdateZoneDto } from './dto/update-zone.dto';
import { AuthGuard } from '../auth/auth.guard';
import { ReqReturnDto } from '../auth/dto/req-return.dto';
import { DevicesService } from '../devices/devices.service';
import {
  ApiBearerAuth,
  ApiBody,
  ApiOperation,
  ApiResponse,
  ApiTags,
} from '@nestjs/swagger';
import { Zone } from './entities/zone.entity';

@UseGuards(AuthGuard)
@ApiTags('Zones')
@ApiBearerAuth()
@Controller('zones')
export class ZonesController {
  constructor(
    private readonly zonesService: ZonesService,
    private readonly devicesService: DevicesService,
  ) {}

  @Post()
  @ApiOperation({ summary: 'Cria uma nova zona.' })
  @ApiBody({ type: CreateZoneDto })
  @HttpCode(HttpStatus.CREATED)
  @UseGuards(AuthGuard)
  create(@Body() createZoneDto: CreateZoneDto) {
    return this.zonesService.create(createZoneDto);
  }

  @Get()
  @ApiOperation({ summary: 'Busca todas as zonas do usuário logado.' })
  @ApiResponse({ status: 200, description: 'Lista de zonas.', type: [Zone] })
  @ApiResponse({ status: 404, description: 'Nenhuma zona encontrada.' })
  @UseGuards(AuthGuard)
  findAll() {
    return this.zonesService.findAll();
  }

  @Get(':id')
  @ApiOperation({ summary: 'Busca uma zona do usuário logado pelo ID.' })
  @ApiResponse({ status: 200, description: 'Zona encontrada.', type: Zone })
  @ApiResponse({ status: 404, description: 'Zona não encontrada.' })
  @UseGuards(AuthGuard)
  findOne(@Param('id') id: string) {
    return this.zonesService.findOne(id);
  }

  @ApiOperation({ summary: 'Atualiza uma zona pelo ID.' })
  @Patch(':id')
  @UseGuards(AuthGuard)
  update(@Param('id') id: string, @Body() updateZoneDto: UpdateZoneDto) {
    return this.zonesService.update(id, updateZoneDto);
  }

  @ApiOperation({ summary: 'Remove uma zona pelo ID.' })
  @Delete(':id')
  @UseGuards(AuthGuard)
  remove(@Param('id') id: string) {
    return this.zonesService.remove(id);
  }

  @ApiOperation({
    summary: 'Busca todos os dispositivos vinculados a uma zona específica.',
  })
  @Get(':zoneId/devices')
  @UseGuards(AuthGuard)
  findDevicesByZone(
    @Param('zoneId') zoneId: string,
    @Request() req: ReqReturnDto,
  ) {
    return this.devicesService.findDevicesByZone(zoneId, req.user.id);
  }
}
