import {
  IsNotEmpty,
  IsString,
  IsIn,
  IsOptional,
  IsInt,
  Min,
  IsBoolean,
  IsDateString,
} from 'class-validator';
import { ApiProperty } from '@nestjs/swagger';

export class ExportDataDto {
  @ApiProperty({
    description: 'The type of entity to export',
    enum: ['mesh', 'zone', 'device'],
  })
  @IsIn(['mesh', 'zone', 'device'])
  @IsString()
  @IsNotEmpty()
  entityType: string;

  @ApiProperty({
    description: 'The ID of the entity to export',
  })
  @IsString()
  @IsNotEmpty()
  entityId: string;

  // New optional controls
  @ApiProperty({
    description: 'Max rows per CSV file (chunk size). Default 100000.',
    required: false,
    example: 100000,
  })
  @IsOptional()
  @IsInt()
  @Min(1000)
  chunkSize?: number;

  @ApiProperty({
    description: 'DB page size per fetch. Default 10000.',
    required: false,
    example: 10000,
  })
  @IsOptional()
  @IsInt()
  @Min(1000)
  pageSize?: number;

  @ApiProperty({
    description: 'ISO start datetime filter for readings (inclusive).',
    required: false,
    example: '2025-01-01T00:00:00.000Z',
  })
  @IsOptional()
  @IsDateString()
  from?: string;

  @ApiProperty({
    description: 'ISO end datetime filter for readings (inclusive).',
    required: false,
    example: '2025-12-31T23:59:59.999Z',
  })
  @IsOptional()
  @IsDateString()
  to?: string;

  @ApiProperty({
    description: 'Whether to include CSV headers in every file. Default true.',
    required: false,
    example: true,
  })
  @IsOptional()
  @IsBoolean()
  includeHeadersPerFile?: boolean;
}
