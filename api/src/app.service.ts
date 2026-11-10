import { Injectable } from '@nestjs/common';
import { MailerService } from '@nestjs-modules/mailer';

@Injectable()
export class AppService {
  constructor(private readonly mailService: MailerService) {}
  getHello(): string {
    return 'Olá mundo!';
  }
}
