import { ApiProperty } from '@nestjs/swagger';
import { IsString } from 'class-validator';

export class CreateZoneDto {
  @IsString()
  @ApiProperty({
    description: 'Nome da zona.',
    example: 'Zona de teste',
  })
  name: string;

  @ApiProperty({
    description: 'Vértices que definem a forma do polígono em um mapa virtual.',
    example: [
      { x: 0, y: -50 },
      { x: 100, y: 50 },
      { x: -100, y: 50 },
    ],
  })
  vertices: { x: number; y: number }[];

  @ApiProperty({
    description: 'ID da malha (mesh) à qual a zona pertence.',
    example: 'a1b2c3d4-e5f6-7890-1234-56789abcdef0',
  })
  meshId: string;

  @ApiProperty({
    description: 'Cor de fundo da zona em hexadecimal.',
    example: '#FF0000',
    required: false,
  })
  bgColor?: string;
}
