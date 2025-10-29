import {
  Controller,
  Post,
  Patch,
  Body,
  Param,
  HttpCode,
  HttpStatus,
  BadRequestException,
  Get,
  Query,
} from '@nestjs/common';
import { UsersService } from './users.service';
import { CreateUserDto } from './dto/create-user.dto';
import { UpdateUserDto } from './dto/update-user.dto';
import { RecoverPasswordDto } from './dto/recover-password.dto';
import { Public } from '../auth/public.decorator';
import { ResetPasswordDto } from './dto/reset-password.dto';
import {
  ApiBearerAuth,
  ApiTags,
  ApiOperation,
  ApiResponse,
  ApiBody,
  ApiParam,
} from '@nestjs/swagger';

@Controller('users')
@ApiTags('Users')
export class UsersController {
  constructor(private readonly usersService: UsersService) {}

  @Public()
  @Post('register')
  @HttpCode(HttpStatus.CREATED)
  @ApiOperation({ summary: 'Registra um novo usuário.' })
  @ApiBody({ type: CreateUserDto })
  @ApiResponse({
    status: 201,
    description: 'Usuário criado com sucesso.',
  })
  async create(@Body() createUserDto: CreateUserDto) {
    const user = await this.usersService.create(
      createUserDto.email,
      createUserDto.password,
      createUserDto.name,
    );

    return {
      message: 'User created successfully',
      user,
    };
  }

  @Patch(':id')
  @ApiOperation({ summary: 'Atualiza os dados de um usuário existente.' })
  @ApiParam({ name: 'id', description: 'ID do usuário a ser atualizado.' })
  @ApiBody({ type: UpdateUserDto })
  @ApiResponse({
    status: 200,
    description: 'Usuário atualizado com sucesso.',
  })
  @ApiBearerAuth()
  async update(@Param('id') id: string, @Body() updateUserDto: UpdateUserDto) {
    // Validate that the DTO exists and has at least one field
    if (!updateUserDto || Object.keys(updateUserDto).length === 0) {
      throw new BadRequestException('Update data is required');
    }

    const user = await this.usersService.update(id, updateUserDto);
    return {
      message: 'User updated successfully',
      user,
    };
  }

  @Public()
  @Post('recover-password')
  @HttpCode(HttpStatus.OK)
  @ApiOperation({ summary: 'Envia um e-mail de recuperação de senha.' })
  @ApiBody({ type: RecoverPasswordDto })
  @ApiResponse({
    status: 200,
    description: 'E-mail de recuperação de senha enviado com sucesso.',
  })
  async recoverPassword(@Body() recoverPasswordDto: RecoverPasswordDto) {
    await this.usersService.recoverPassword(recoverPasswordDto.email);
    return {
      message: 'Password recovery email sent successfully',
    };
  }

  @Public()
  @Get('reset-password/check')
  @HttpCode(HttpStatus.OK)
  @ApiOperation({ summary: 'Valida o token de recuperação de senha.' })
  @ApiResponse({ status: 200, description: 'Token válido.' })
  @ApiResponse({ status: 400, description: 'Token inválido ou expirado.' })
  async checkResetToken(@Query('token') token: string) {
    const valid = await this.usersService.verifyResetToken(token);
    if (!valid) {
      throw new BadRequestException('Invalid or expired token');
    }
    return { valid: true };
  }

  @Public()
  @Post('reset-password')
  @HttpCode(HttpStatus.OK)
  @ApiOperation({
    summary: 'Redefine a senha usando o token recebido por e-mail.',
  })
  @ApiBody({ type: ResetPasswordDto })
  @ApiResponse({ status: 200, description: 'Senha redefinida com sucesso.' })
  async resetPassword(@Body() dto: ResetPasswordDto) {
    await this.usersService.resetPassword(dto.token, dto.newPassword);
    return { message: 'Password updated successfully' };
  }
}
