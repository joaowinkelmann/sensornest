import {
  Injectable,
  ConflictException,
  BadRequestException,
} from '@nestjs/common';
import { PrismaService } from '../../prisma/prisma.service';
import { UpdateUserDto } from './dto/update-user.dto';
import * as argon2 from 'argon2';
import * as crypto from 'crypto';
import { ValidatePasswordDto } from './dto/validate-password.dto';
import { MailerService } from '../mailer/mailer.service';

@Injectable()
export class UsersService {
  constructor(
    private readonly prismaService: PrismaService,
    private readonly mailerService: MailerService,
  ) {}

  async findOne(username: string) {
    return await this.prismaService.user.findUnique({
      where: { email: username },
      select: {
        id: true,
        email: true,
        name: true,
      },
    });
  }

  async findByEmail(email: string) {
    return await this.prismaService.user.findUnique({
      where: { email },
    });
  }

  async create(email: string, password: string, name?: string) {
    // Check if user already exists
    const existingUser = await this.findByEmail(email);
    if (existingUser) {
      throw new ConflictException('User with this email already exists');
    }

    // Hash the password
    const hashedPassword = await argon2.hash(password);

    // Create the user
    return await this.prismaService.user.create({
      data: {
        email,
        passwordHash: hashedPassword,
        name,
      },
      select: {
        id: true,
        email: true,
        name: true,
        insDthr: true,
        altDthr: true,
      },
    });
  }

  async validatePassword(
    email: string,
    password: string,
  ): Promise<false | ValidatePasswordDto> {
    const user = await this.findByEmail(email);
    if (!user) {
      return false;
    }

    const isPasswordValid = await argon2.verify(user.passwordHash, password);
    if (!isPasswordValid) {
      return false;
    }
    return {
      id: user.id,
      email: user.email,
      name: user.name,
    };
  }

  async update(id: string, updateUserDto: UpdateUserDto) {
    if (Object.keys(updateUserDto).length === 0) {
      throw new BadRequestException('No fields to update');
    }

    return await this.prismaService.user.update({
      where: { id },
      data: updateUserDto,
      select: {
        id: true,
        email: true,
        name: true,
        altDthr: true,
        insDthr: true,
      },
    });
  }

  async recoverPassword(email: string) {
    const user = await this.findByEmail(email);
    if (!user) {
      return;
    }

    const token = crypto.randomBytes(20).toString('hex');
    const now = new Date();
    now.setHours(now.getHours() + 1);

    await this.prismaService.user.update({
      where: { email },
      data: {
        passwordResetToken: token,
        passwordResetExpires: now,
      },
    });

    await this.mailerService.sendPasswordRecoveryEmail(email, token);
  }
}
