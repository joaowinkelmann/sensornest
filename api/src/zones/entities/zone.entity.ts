import { ApiProperty } from '@nestjs/swagger';
import {
  IsArray,
  IsOptional,
  IsString,
  IsUUID,
  IsDateString,
} from 'class-validator';

export class Zone {
  @ApiProperty({
    description: 'ID da Zone.',
    example: 'a1b2c3d4-e5f6-7890-1234-56789abcdef0',
  })
  @IsUUID()
  @IsOptional()
  id?: string;

  @ApiProperty({ description: 'Nome da Zone.', example: 'Zona de teste' })
  @IsString()
  name: string;

  @ApiProperty({
    description: 'Data de inserção (audit).',
    example: '2025-08-25T12:34:56.000Z',
  })
  @IsDateString()
  @IsOptional()
  insDthr?: string;

  @ApiProperty({
    description: 'Data da última alteração (audit).',
    example: '2025-08-25T13:00:12.000Z',
  })
  @IsDateString()
  @IsOptional()
  altDthr?: string;

  @ApiProperty({
    description: 'Vértices do polígono (JSON armazenado no banco).',
    example: [
      { x: 0, y: -50 },
      { x: 100, y: 50 },
      { x: -100, y: 50 },
    ],
  })
  @IsArray()
  vertices: { x: number; y: number }[];

  @ApiProperty({
    description: 'ID da Mesh à qual a Zone pertence.',
    example: 'a1b2c3d4-e5f6-7890-1234-56789abcdef0',
  })
  @IsString()
  meshId: string;

  @ApiProperty({
    description: 'Cor de fundo hexadecimal.',
    example: '#FF0000',
    required: false,
  })
  @IsOptional()
  @IsString()
  bgColor?: string;
}
