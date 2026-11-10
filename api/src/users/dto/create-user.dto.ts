import {
  IsEmail,
  IsNotEmpty,
  IsOptional,
  IsString,
  MinLength,
} from 'class-validator';
import { ApiProperty } from '@nestjs/swagger';

export class CreateUserDto {
  @ApiProperty({
    example: 'test@example.com',
    description: 'E-mail do usuário',
  })
  @IsEmail()
  email: string;

  @ApiProperty({ example: 'password123', description: 'Senha do usuário' })
  @IsString()
  @IsNotEmpty()
  @MinLength(6)
  password: string;

  @ApiProperty({
    example: 'Kléber da Silva Santiago',
    description: 'Nome do usuário',
    required: false,
  })
  @IsString()
  @IsOptional()
  name: string;
}
