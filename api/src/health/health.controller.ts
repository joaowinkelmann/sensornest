import { Controller, Get } from '@nestjs/common';
import {
  HealthCheckService,
  HealthCheck,
  PrismaHealthIndicator,
} from '@nestjs/terminus';
import { PrismaService } from '../../prisma/prisma.service';
import { Public } from '../auth/public.decorator';
import { ApiOperation } from '@nestjs/swagger';

@Controller('health')
export class HealthController {
  constructor(
    private health: HealthCheckService,
    private prismaHealthIndicator: PrismaHealthIndicator,
    private prismaService: PrismaService,
  ) {}

  @Public()
  @Get()
  @ApiOperation({
    summary: 'Verifica a saúde da API, incluindo o banco de dados.',
  })
  @HealthCheck()
  healthCheck() {
    return this.health.check([
      () =>
        this.prismaHealthIndicator.pingCheck('prisma', this.prismaService, {
          timeout: 3000,
        }),
    ]);
  }
}
