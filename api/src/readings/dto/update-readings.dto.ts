import { PartialType } from '@nestjs/swagger';
import { CreateReadingsDto } from './create-readings.dto';

export class UpdateReadingsDto extends PartialType(CreateReadingsDto) {}
