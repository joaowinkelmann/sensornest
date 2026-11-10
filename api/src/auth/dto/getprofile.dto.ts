import { IsDefined } from 'class-validator';
import { ApiProperty } from '@nestjs/swagger';

export class UserProfileDto {
  @ApiProperty({ example: 'user-id-123', description: 'Uuid do usuário.' })
  id: string;

  @ApiProperty({
    example: 'test@example.com',
    description: 'E-mail do usuário.',
  })
  email: string;

  @ApiProperty({
    example: 'Kléber da Silva Santiago',
    description: 'Nome do usuário',
    required: false,
  })
  name: string;
}

export class GetProfileDto {
  @ApiProperty({
    type: UserProfileDto,
    description: 'Payload do usuário autenticado.',
  })
  @IsDefined()
  user: UserProfileDto;
}
