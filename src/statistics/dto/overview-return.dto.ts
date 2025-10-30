import { ApiProperty } from '@nestjs/swagger';

class DevicesOverviewDto {
  @ApiProperty()
  active: number;

  @ApiProperty()
  inactive: number;

  @ApiProperty()
  total: number;
}

class ReadingsOverviewDto {
  @ApiProperty()
  allTime: number;

  @ApiProperty()
  lastHour: number;

  @ApiProperty()
  lastDay: number;
}

class ReadingsPerMeshDto {
  @ApiProperty()
  meshId: string;

  @ApiProperty()
  meshName: string;

  @ApiProperty()
  count: number;
}

export class OverviewReturnDto {
  @ApiProperty({ type: DevicesOverviewDto })
  devices: DevicesOverviewDto;

  @ApiProperty({ type: ReadingsOverviewDto })
  readings: ReadingsOverviewDto;

  @ApiProperty({ type: [ReadingsPerMeshDto] })
  readingsPerMesh: ReadingsPerMeshDto[];
}
