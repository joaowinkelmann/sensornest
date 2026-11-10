import {
  Controller,
  Post,
  Body,
  HttpCode,
  HttpStatus,
  Delete,
  Param,
} from '@nestjs/common';
import { ReadingsService } from './readings.service';
import { CreateReadingsDto } from './dto/create-readings.dto';
import {
  ApiBearerAuth,
  ApiBody,
  ApiOperation,
  ApiResponse,
  ApiTags,
} from '@nestjs/swagger';

@Controller('readings')
@ApiTags('Readings')
export class ReadingsController {
  constructor(private readonly readingsService: ReadingsService) {}

  @Post('add')
  @ApiBearerAuth()
  @ApiOperation({
    summary:
      'Registra novas leituras de sensores. Pode ser passado um array de leituras.',
  })
  @ApiResponse({
    status: 201,
    description: 'Registros criados com sucesso.',
    schema: {
      example: {
        message: '3 registros criados com sucesso.',
        createdCount: 3,
        unregisteredCount: 1,
      },
    },
  })
  @ApiBody({ type: CreateReadingsDto })
  @HttpCode(HttpStatus.CREATED) // HTTP 201 se conseguiu criar
  async createReadings(@Body() createReadingsDto: CreateReadingsDto) {
    const result = await this.readingsService.createReadings(createReadingsDto);
    return {
      message: `${result.createdCount} registros criados com sucesso.`,
      ...result,
    };
  }

  @Delete(':id')
  @ApiBearerAuth()
  @ApiOperation({ summary: 'Deleta uma leitura pelo ID.' })
  @ApiResponse({
    status: 200,
    description: 'Leitura deletada com sucesso.',
    schema: {
      example: {
        message: 'Leitura deletada com sucesso.',
      },
    },
  })
  @ApiResponse({
    status: 404,
    description: 'Leitura não encontrada.',
    schema: {
      example: {
        statusCode: 404,
        message: 'Leitura não encontrada',
        error: 'Not Found',
      },
    },
  })
  @HttpCode(HttpStatus.OK)
  async remove(@Param('id') id: string) {
    await this.readingsService.remove(id);
    return {
      message: 'Leitura deletada com sucesso.',
    };
  }
}
