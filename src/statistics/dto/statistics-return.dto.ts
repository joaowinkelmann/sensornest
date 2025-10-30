import { ApiProperty } from '@nestjs/swagger';

class Stats {
  @ApiProperty()
  highestTemperature: number;

  @ApiProperty()
  highestHumidity: number;

  @ApiProperty()
  averageTemperature: number;

  @ApiProperty()
  averageHumidity: number;
}

export class StatisticsReturnDto {
  @ApiProperty({ type: Stats })
  day: Stats;

  @ApiProperty({ type: Stats })
  week: Stats;

  @ApiProperty({ type: Stats })
  month: Stats;

  @ApiProperty({ type: Stats })
  year: Stats;
}
