import { ApiProperty } from '@nestjs/swagger';
import { IsMACAddress, IsNumber, IsOptional } from 'class-validator';

export class SensorReadingInput {
  @ApiProperty({ example: 'AA:BB:CC:DD:EE:01' })
  @IsMACAddress()
  mac: string;

  @ApiProperty({ example: 25.5 })
  @IsNumber()
  temp: number;

  @ApiProperty({ example: 60.2 })
  @IsNumber()
  hum: number;

  @ApiProperty({
    example: '2025-08-25T12:34:56.000Z',
    required: false,
    description:
      'Opcional: se não enviado, servidor usa o horário de processamento',
  })
  @IsOptional()
  readingTime?: string;
}
