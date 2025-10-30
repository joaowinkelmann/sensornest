import {
  Controller,
  Post,
  Body,
  UseGuards,
  Request,
  Logger,
} from '@nestjs/common';
import { DataExporterService } from './data-exporter.service';
import { ExportDataDto } from './dto/export-data.dto';
import { AuthGuard } from '../auth/auth.guard';
import { ReqReturnDto } from '../auth/dto/req-return.dto';
import {
  ApiBearerAuth,
  ApiTags,
  ApiOperation,
  ApiResponse,
  ApiBody,
} from '@nestjs/swagger';

@UseGuards(AuthGuard)
@ApiBearerAuth()
@ApiTags('DataExporter')
@Controller('data-exporter')
export class DataExporterController {
  private readonly logger = new Logger(DataExporterController.name);

  constructor(private readonly dataExporterService: DataExporterService) {}

  @Post('export')
  @ApiOperation({ summary: 'Export data for a given entity to a CSV file.' })
  @ApiBody({ type: ExportDataDto })
  @ApiResponse({
    status: 201,
    description: 'The URL of the exported data file.',
    type: String,
  })
  exportData(
    @Body() exportDataDto: ExportDataDto,
    @Request() req: ReqReturnDto,
  ) {
    this.logger.log(
      `Export requested for entity ${exportDataDto.entityType} with id ${exportDataDto.entityId} by user ${req.user.id}`,
    );
    return this.dataExporterService.exportData(exportDataDto, req.user.id);
  }
}
