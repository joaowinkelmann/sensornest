import { ApiProperty } from '@nestjs/swagger';
import { IsUUID, IsOptional, IsNumber, IsDateString } from 'class-validator';

export class Reading {
  @ApiProperty({ example: 'f3d9a7b2-1c4e-4b89-8d21-9a7e6c5b4d32' })
  @IsUUID()
  id: string;

  @ApiProperty({ example: '2025-08-25T12:34:56.000Z' })
  @IsDateString()
  readingTime: string;

  @ApiProperty({ example: 25.5, required: false })
  @IsOptional()
  @IsNumber()
  temperature?: number | null;

  @ApiProperty({ example: 60.2, required: false })
  @IsOptional()
  @IsNumber()
  humidity?: number | null;

  @ApiProperty({ example: 'device-id-123', required: false })
  @IsOptional()
  @IsUUID()
  deviceId?: string | null;

  @ApiProperty({ example: '2025-08-25T12:34:56.000Z' })
  @IsDateString()
  insDthr: string;

  @ApiProperty({ example: '2025-08-25T13:10:11.000Z' })
  @IsDateString()
  altDthr: string;
}
