import { ApiProperty } from '@nestjs/swagger';

class GraphData {
  @ApiProperty()
  bucket: string;

  @ApiProperty()
  avgTemperature: number;

  @ApiProperty()
  avgHumidity: number;
}

export class GraphReturnDto {
  @ApiProperty({ type: [GraphData] })
  day: GraphData[];

  @ApiProperty({ type: [GraphData] })
  week: GraphData[];

  @ApiProperty({ type: [GraphData] })
  month: GraphData[];
}
