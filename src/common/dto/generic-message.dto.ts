import { ApiProperty } from '@nestjs/swagger';

export class GenericMessageDto {
    @ApiProperty({
        example: 'Success',
        description: 'Mensagem de resposta.'
    })
    message: string;
}
