import { IsNotEmpty, IsString, IsIn } from 'class-validator';
import { ApiProperty } from '@nestjs/swagger';

export class ExportDataDto {
  @ApiProperty({
    description: 'The type of entity to export',
    enum: ['mesh', 'zone', 'device'],
  })
  @IsIn(['mesh', 'zone', 'device'])
  @IsString()
  @IsNotEmpty()
  entityType: string;

  @ApiProperty({
    description: 'The ID of the entity to export',
  })
  @IsString()
  @IsNotEmpty()
  entityId: string;
}
