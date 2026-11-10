import { IsEmail } from 'class-validator';
import { ApiProperty } from '@nestjs/swagger';

export class RecoverPasswordDto {
  @ApiProperty({
    example: 'test@example.com',
    description: 'E-mail do usuário',
  })
  @IsEmail()
  email: string;
}
