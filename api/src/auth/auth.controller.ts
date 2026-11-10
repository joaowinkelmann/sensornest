import {
  Body,
  Controller,
  Get,
  HttpCode,
  HttpStatus,
  NotFoundException,
  Post,
  Request,
  UseGuards,
} from '@nestjs/common';
import { AuthService } from './auth.service';
import { AuthGuard } from './auth.guard';
import { SignInDto } from './dto/signin.dto';
import { Public } from './public.decorator';
import { GetProfileDto } from './dto/getprofile.dto';
import {
  ApiTags,
  ApiOperation,
  ApiBody,
  ApiResponse,
  ApiBearerAuth,
} from '@nestjs/swagger';

@Controller('auth')
@ApiTags('Auth')
export class AuthController {
  constructor(private authService: AuthService) {}

  @HttpCode(HttpStatus.OK)
  @Public()
  @Post('login')
  @ApiOperation({
    summary: 'Login do usuário - Retorna um JWT access_token',
  })
  @ApiBody({ type: SignInDto })
  @ApiResponse({
    status: 200,
    description:
      'Login realizado com sucesso, junto com um JWT access_token para ser utilizado.',
  })
  @ApiResponse({ status: 401, description: 'Credenciais inválidas.' })
  async signIn(@Body() signInDto: SignInDto) {
    return this.authService.signIn(signInDto.email, signInDto.password);
  }

  @UseGuards(AuthGuard)
  @Get('profile')
  @ApiBearerAuth()
  @ApiOperation({ summary: 'Busca o perfil do usuário autenticado' })
  @ApiResponse({
    status: 200,
    description: 'Retorna o usuário vinculado ao JWT válido informado.',
    type: GetProfileDto,
  })
  @ApiResponse({ status: 404, description: 'Usuário não encontrado.' })
  getProfile(@Request() req: GetProfileDto) {
    if (!req.user) {
      throw new NotFoundException('User not found');
    }
    return req.user;
  }
}
