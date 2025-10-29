import { Module } from '@nestjs/common';
import { UsersService } from './users.service';
import { UsersController } from './users.controller';
import { PrismaService } from '../../prisma/prisma.service';
import { MailerServiceModule } from '../../mailer/mailer.module';

@Module({
  imports: [MailerServiceModule],
  controllers: [UsersController],
  providers: [UsersService, PrismaService],
  exports: [UsersService, PrismaService],
})
export class UsersModule {}
