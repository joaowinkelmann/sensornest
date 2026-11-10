import { ApiProperty } from '@nestjs/swagger';

export class CheckResetTokenResponseDto {
  @ApiProperty({
    example: true,
    description: 'Indica se o token é válido.',
  })
  valid: boolean;
}
