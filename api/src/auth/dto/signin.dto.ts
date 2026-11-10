import { IsEmail, IsNotEmpty, IsString } from 'class-validator';
import { ApiProperty } from '@nestjs/swagger';
import { ValidatePasswordDto } from '../../users/dto/validate-password.dto';

export class SignInDto {
  @ApiProperty({
    example: 'test@example.com',
    description: 'E-mail do usuário',
  })
  @IsEmail()
  email: string;

  @ApiProperty({ example: 'password123', description: 'Senha do usuário' })
  @IsNotEmpty()
  password: string;
}

export class SignInResponseDto {
  @ApiProperty()
  user: ValidatePasswordDto;

  @ApiProperty({ example: 'your-jwt-token' })
  @IsString()
  @IsNotEmpty()
  access_token: string;
}
