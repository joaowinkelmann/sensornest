import { IsEmail, IsNotEmpty, IsString } from 'class-validator';
import { ApiProperty } from '@nestjs/swagger';

export class ValidatePasswordDto {
  @ApiProperty({
    example: 'Kléber da Silva Santiago',
    description: 'Nome do usuário',
  })
  @IsString()
  @IsNotEmpty()
  name: string;

  @ApiProperty({ example: 'test@example.com', description: 'User email' })
  @IsString()
  @IsEmail()
  @IsNotEmpty()
  email: string;

  @ApiProperty({ example: 'user-id-123', description: 'User ID' })
  @IsString()
  @IsNotEmpty()
  id: string;
}
