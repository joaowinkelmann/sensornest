import { ApiProperty } from '@nestjs/swagger';
import { DeviceRole, DeviceStatus } from '@prisma/client';
import {
  IsUUID,
  IsOptional,
  IsString,
  IsNumber,
  IsEnum,
  IsInt,
  Min,
  IsDate,
} from 'class-validator';

export class Device {
  @ApiProperty({ example: 'a9e5d7c1-1234-4f8e-9b3e-5f4b1e7d2d11' })
  @IsUUID()
  id: string;

  @ApiProperty({ example: '24:58:7C:CC:E5:B4' })
  @IsString()
  macAddress: string;

  @ApiProperty({ example: 'Dispositivo Controlador Seed' })
  @IsString()
  name: string;

  @ApiProperty({
    example: 'Controlador principal com acesso à rede AP',
    required: false,
  })
  @IsOptional()
  @IsString()
  description?: string | null;

  @ApiProperty({ example: 5 })
  @IsNumber()
  x: number;

  @ApiProperty({ example: 5 })
  @IsNumber()
  y: number;

  @ApiProperty({ example: DeviceStatus.ACTIVE, enum: DeviceStatus })
  @IsEnum(DeviceStatus)
  status: DeviceStatus;

  @ApiProperty({ example: DeviceRole.CONTROLLER, enum: DeviceRole })
  @IsEnum(DeviceRole)
  role: DeviceRole;

  @ApiProperty({ example: 'zone-id-123', required: false })
  @IsOptional()
  @IsString()
  zoneId?: string | null;

  @ApiProperty({ example: 'mesh-123', required: false })
  @IsOptional()
  @IsString()
  meshId?: string | null;

  @ApiProperty({ example: '#FF0000', required: false })
  @IsOptional()
  @IsString()
  deviceColor?: string | null;

  @ApiProperty({ example: 'user-id-123' })
  @IsString()
  userId: string;

  @ApiProperty({ example: 2, required: false })
  @IsOptional()
  @IsInt()
  @Min(1)
  readingsPerBatch?: number | null;

  @ApiProperty({ example: 3600, required: false })
  @IsOptional()
  @IsInt()
  @Min(1)
  wakeUpInterval?: number | null;

  @ApiProperty({
    example: '2025-08-25T12:34:56.000Z',
    type: String,
    format: 'date-time',
    readOnly: true,
  })
  @IsDate()
  insDthr: Date;

  @ApiProperty({
    example: '2025-08-25T13:45:12.000Z',
    type: String,
    format: 'date-time',
    readOnly: true,
  })
  @IsDate()
  altDthr: Date;
}
