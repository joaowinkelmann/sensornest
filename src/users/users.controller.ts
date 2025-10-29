import {
  Controller,
  Post,
  Patch,
  Body,
  Param,
  HttpCode,
  HttpStatus,
  BadRequestException,
} from '@nestjs/common';
import { UsersService } from './users.service';
import { CreateUserDto } from './dto/create-user.dto';
import { UpdateUserDto } from './dto/update-user.dto';
import { RecoverPasswordDto } from './dto/recover-password.dto';
import { Public } from '../auth/public.decorator';
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
}
