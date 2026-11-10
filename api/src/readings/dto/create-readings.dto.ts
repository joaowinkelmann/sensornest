import { IsArray, ValidateNested } from 'class-validator';
import { Type } from 'class-transformer';
import { ApiProperty } from '@nestjs/swagger';
import { SensorReadingInput } from '../entities/sensor-reading-input.entity';

export class CreateReadingsDto {
  @ApiProperty({
    description: 'Um array de leituras de sensores.',
    type: [SensorReadingInput],
  })
  @IsArray()
  @ValidateNested({ each: true })
  @Type(() => SensorReadingInput)
  data: SensorReadingInput[];
}
