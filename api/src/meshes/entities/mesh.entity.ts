import { ApiProperty } from '@nestjs/swagger';
import {
  IsUUID,
  IsString,
  IsNumber,
  IsOptional,
  IsDateString,
} from 'class-validator';

export class Mesh {
  @ApiProperty({ example: 'c2c0a8a9-45fd-4f8e-9b3e-5f4b1e7d2d11' })
  @IsUUID()
  id: string;

  @ApiProperty({ example: 'Mesh Principal' })
  @IsString()
  name: string;

  @ApiProperty({ example: '2025-08-25T12:34:56.000Z' })
  @IsDateString()
  insDthr: string;

  @ApiProperty({ example: '2025-08-25T13:12:34.000Z' })
  @IsDateString()
  altDthr: string;

  @ApiProperty({ example: -23.5505, description: 'Latitude do centro' })
  @IsNumber()
  lat: number;

  @ApiProperty({ example: -46.6333, description: 'Longitude do centro' })
  @IsNumber()
  lon: number;

  @ApiProperty({
    example: 'https://cdn.example.com/maps/custom/{z}/{x}/{y}.png',
    required: false,
  })
  @IsOptional()
  @IsString()
  mapUrl?: string;

  @IsOptional()
  @IsNumber()
  mapMaxZoom?: number;

  @IsOptional()
  @IsNumber()
  mapMinZoom?: number;

  @ApiProperty({ example: 'b7a5ad7e-0c2f-4c1b-93d1-b0f6f9d9c111' })
  @IsUUID()
  userId: string;
}
