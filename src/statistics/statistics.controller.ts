import { Controller, Get, Param, UseGuards } from '@nestjs/common';
import { StatisticsService } from './statistics.service';
import { AuthGuard } from '../auth/auth.guard';
import {
  ApiBearerAuth,
  ApiOperation,
  ApiResponse,
  ApiTags,
} from '@nestjs/swagger';

@ApiBearerAuth()
@ApiTags('Statistics')
@Controller('statistics')
export class StatisticsController {
  constructor(private readonly statisticsService: StatisticsService) {}

  @ApiOperation({ summary: 'Busca estatísticas para um dispositivo' })
  @ApiResponse({ status: 200, description: 'Return device statistics.' })
  @UseGuards(AuthGuard)
  @Get('device/:id')
  async getDeviceStatistics(@Param('id') id: string) {
    return this.statisticsService.getDeviceStatistics(id);
  }

  @ApiOperation({ summary: 'Busca estatísticas para uma Zone' })
  @ApiResponse({ status: 200, description: 'Return zone statistics.' })
  @UseGuards(AuthGuard)
  @Get('zone/:id')
  async getZoneStatistics(@Param('id') id: string) {
    return this.statisticsService.getZoneStatistics(id);
  }

  @ApiOperation({ summary: 'Busca estatísticas para uma Mesh' })
  @ApiResponse({ status: 200, description: 'Return mesh statistics.' })
  @UseGuards(AuthGuard)
  @Get('mesh/:id')
  async getMeshStatistics(@Param('id') id: string) {
    return this.statisticsService.getMeshStatistics(id);
  }
}
