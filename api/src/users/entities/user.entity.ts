import { ApiProperty } from '@nestjs/swagger';
import {
  IsEmail,
  IsOptional,
  IsString,
  IsUUID,
  IsDateString,
} from 'class-validator';

export class User {
  @ApiProperty({ example: 'b7a5ad7e-0c2f-4c1b-93d1-b0f6f9d9c111' })
  @IsUUID()
  id: string;

  @ApiProperty({ example: 'user@example.com' })
  @IsEmail()
  email: string;

  @ApiProperty({ example: 'Kléber da Silva Santiago' })
  @IsString()
  name: string;

  @ApiProperty({ example: '2025-08-25T12:34:56.000Z' })
  @IsDateString()
  insDthr: string;

  @ApiProperty({ example: '2025-08-25T13:22:11.000Z' })
  @IsDateString()
  altDthr: string;

  // passwordHash intentionally omitted
  @IsOptional()
  meshes?: any[];

  @IsOptional()
  devices?: any[];
}
