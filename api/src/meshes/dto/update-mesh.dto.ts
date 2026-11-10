import { PartialType } from '@nestjs/swagger';
import { CreateMeshDto } from './create-mesh.dto';

export class UpdateMeshDto extends PartialType(CreateMeshDto) {}
