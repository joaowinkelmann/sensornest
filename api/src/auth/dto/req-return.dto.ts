import { ApiProperty } from '@nestjs/swagger';

class UserPayload {
  @ApiProperty({ example: 'user-id-123' })
  id: string;

  @ApiProperty({ example: 'test@example.com' })
  email: string;

  @ApiProperty({ example: 1615384800 })
  iat: number;

  @ApiProperty({ example: 1615471200 })
  exp: number;
}

export class ReqReturnDto {
  @ApiProperty({ type: UserPayload })
  user: UserPayload;
}
