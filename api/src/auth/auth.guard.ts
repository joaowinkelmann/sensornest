import { Injectable, ExecutionContext } from '@nestjs/common';
import { AuthGuard as NestPassportAuthGuard } from '@nestjs/passport';
import { Reflector } from '@nestjs/core';
import { IS_PUBLIC_KEY } from './public.decorator';

const JwtAuthGuardBase = NestPassportAuthGuard('jwt');

@Injectable()
export class AuthGuard extends JwtAuthGuardBase {
  constructor(private reflector: Reflector) {
    super();
  }

  canActivate(context: ExecutionContext) {
    const isPublic = this.reflector.getAllAndOverride<boolean>(IS_PUBLIC_KEY, [
      context.getHandler(),
      context.getClass(),
    ]);
    if (isPublic) {
      return true;
    }
    return super.canActivate(context);
  }
}
