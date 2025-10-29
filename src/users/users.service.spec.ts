import { Test, TestingModule } from '@nestjs/testing';
import { UsersService } from './users.service';
import { PrismaService } from '../../prisma/prisma.service';

import { MailerService } from 'src/mailer/mailer.service';

describe('UsersService', () => {
  let service: UsersService;

  beforeEach(async () => {
    const module: TestingModule = await Test.createTestingModule({
      providers: [
        UsersService,
        { provide: PrismaService, useValue: {} },
        {
          provide: MailerService,
          useValue: { sendPasswordRecoveryEmail: jest.fn() },
        },
      ],
    }).compile();

    service = module.get<UsersService>(UsersService);
  });

  it('should be defined', () => {
    expect(service).toBeDefined();
  });

  describe('recoverPassword', () => {
    it('should send a password recovery email if the user exists', async () => {
      const email = 'test@example.com';
      const user = { email, id: '1' };
      const prismaService = {
        user: {
          findUnique: jest.fn().mockResolvedValue(user),
          update: jest.fn(),
        },
      };
      const mailerService = {
        sendPasswordRecoveryEmail: jest.fn(),
      };
      const module: TestingModule = await Test.createTestingModule({
        providers: [
          UsersService,
          { provide: PrismaService, useValue: prismaService },
          { provide: MailerService, useValue: mailerService },
        ],
      }).compile();
      service = module.get<UsersService>(UsersService);

      await service.recoverPassword(email);

      expect(prismaService.user.findUnique).toHaveBeenCalledWith({
        where: { email },
      });
      expect(prismaService.user.update).toHaveBeenCalled();
      expect(mailerService.sendPasswordRecoveryEmail).toHaveBeenCalled();
    });

    it('should not send a password recovery email if the user does not exist', async () => {
      const email = 'test@example.com';
      const prismaService = {
        user: {
          findUnique: jest.fn().mockResolvedValue(null),
          update: jest.fn(),
        },
      };
      const mailerService = {
        sendPasswordRecoveryEmail: jest.fn(),
      };
      const module: TestingModule = await Test.createTestingModule({
        providers: [
          UsersService,
          { provide: PrismaService, useValue: prismaService },
          { provide: MailerService, useValue: mailerService },
        ],
      }).compile();
      service = module.get<UsersService>(UsersService);

      await service.recoverPassword(email);

      expect(prismaService.user.findUnique).toHaveBeenCalledWith({
        where: { email },
      });
      expect(prismaService.user.update).not.toHaveBeenCalled();
      expect(mailerService.sendPasswordRecoveryEmail).not.toHaveBeenCalled();
    });
  });
});
