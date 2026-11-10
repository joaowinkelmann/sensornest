import {
  IsString,
  IsOptional,
  IsNumber,
  IsEnum,
  IsMACAddress,
  IsInt,
  Min,
} from 'class-validator';
import { DeviceStatus, DeviceRole } from '@prisma/client';
import { ApiProperty } from '@nestjs/swagger';

export class CreateDeviceDto {
  @ApiProperty({
    example: '00:0A:E6:3E:FD:E1',
    description: 'Endereço MAC do dispositivo',
  })
  @IsMACAddress()
  macAddress: string;

  @ApiProperty({ example: 'Meu ESP32', description: 'Nome do dispositivo' })
  @IsString()
  name: string;

  @ApiProperty({
    example: 'Um dispositivo na sala de estar',
    description: 'Descrição breve do dispositivo',
    required: false,
  })
  @IsOptional()
  @IsString()
  description?: string;

  @ApiProperty({
    example: 10.5,
    description:
      'Coordenada X relativa ao centro da Mesh do dispositivo no mapa',
  })
  @IsNumber()
  x: number;

  @ApiProperty({
    example: 20.5,
    description:
      'Coordenada Y relativa ao centro da Mesh do dispositivo no mapa',
  })
  @IsNumber()
  y: number;

  @ApiProperty({
    enum: DeviceStatus,
    example: DeviceStatus.ACTIVE,
    description: 'Status do dispositivo',
    required: false,
  })
  @IsOptional()
  @IsEnum(DeviceStatus)
  status?: DeviceStatus;

  @ApiProperty({
    enum: DeviceRole,
    example: DeviceRole.WORKER,
    description: 'Cargo do dispositivo',
    required: false,
  })
  @IsOptional()
  @IsEnum(DeviceRole)
  role?: DeviceRole;

  @ApiProperty({
    example: 'zone-id-123',
    description: 'ID da zona onde o dispositivo está localizado',
    required: false,
  })
  @IsOptional()
  @IsString()
  zoneId?: string;

  @ApiProperty({
    example: '#FFFFFF',
    description: 'Cor representando o dispositivo no mapa',
    required: false,
  })
  @IsOptional()
  @IsString()
  deviceColor?: string;

  @ApiProperty({
    example: 'mesh-id-123',
    description: 'ID da mesh que o dispositivo pertence',
  })
  @IsString()
  meshId: string;

  @ApiProperty({
    example: 10,
    description: 'Número de leituras por lote enviado',
    required: false,
  })
  @IsOptional()
  @IsInt()
  @Min(1)
  readingsPerBatch?: number;

  @ApiProperty({
    example: 3600,
    description:
      'Intervalo de wake-up em segundos do dispositivo. (Tempo de sono do dispositivo)',
    required: false,
  })
  @IsOptional()
  @IsInt()
  @Min(1)
  wakeUpInterval?: number;
}
