import { ApiProperty } from '@nestjs/swagger';

export class ExportResultDto {
  @ApiProperty({ description: 'Entity type exported.' })
  entityType: string;

  @ApiProperty({ description: 'Entity ID exported.' })
  entityId: string;

  @ApiProperty({ description: 'Total rows exported.' })
  totalRows: number;

  @ApiProperty({ description: 'Rows per CSV file (chunk size).' })
  chunkSize: number;

  @ApiProperty({ description: 'Number of files produced.' })
  fileCount: number;

  @ApiProperty({
    description: 'Array of URLs to the generated CSV chunks, in order.',
    type: [String],
  })
  files: string[];

  @ApiProperty({
    description: 'Optional ISO start datetime filter applied.',
    required: false,
    nullable: true,
  })
  from?: string | null;

  @ApiProperty({
    description: 'Optional ISO end datetime filter applied.',
    required: false,
    nullable: true,
  })
  to?: string | null;
}
