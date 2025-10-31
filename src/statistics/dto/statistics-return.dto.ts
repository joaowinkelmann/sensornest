import { ApiProperty } from '@nestjs/swagger';

export class Stats {
  @ApiProperty({ nullable: true })
  highestTemperature: number | null;

  @ApiProperty({ nullable: true })
  highestHumidity: number | null;

  @ApiProperty({ nullable: true })
  averageTemperature: number | null;

  @ApiProperty({ nullable: true })
  averageHumidity: number | null;
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
