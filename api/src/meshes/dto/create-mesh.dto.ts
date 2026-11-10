import { IsString, IsNumber } from 'class-validator';
import { ApiProperty } from '@nestjs/swagger';

export class CreateMeshDto {
  @ApiProperty({ example: 'Minha Mesh', description: 'Nome da Mesh' })
  @IsString()
  name: string;

  @ApiProperty({
    example: -23.55052,
    description: 'Latitude do centro da mesh',
  })
  @IsNumber()
  lat: number;

  @ApiProperty({
    example: -46.633308,
    description: 'Longitude do centro da mesh',
  })
  @IsNumber()
  lon: number;
}
