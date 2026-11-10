import { Test, TestingModule } from '@nestjs/testing';
import { AuthGuard } from './auth.guard';
import { Reflector } from '@nestjs/core';
import { ExecutionContext } from '@nestjs/common';

class MockReflector {
  getAllAndOverride<T>(_key: string, _targets: any[]): T | undefined {
    return undefined;
  }
}

describe('AuthGuard', () => {
  let guard: AuthGuard;
  let reflector: Reflector;

  beforeEach(async () => {
    const module: TestingModule = await Test.createTestingModule({
      providers: [AuthGuard, { provide: Reflector, useClass: MockReflector }],
    }).compile();

    guard = module.get<AuthGuard>(AuthGuard);
    reflector = module.get<Reflector>(Reflector);
  });

  it('should be defined', () => {
    expect(guard).toBeDefined();
  });

  it('should allow public routes', () => {
    jest.spyOn(reflector, 'getAllAndOverride').mockReturnValue(true);
    const context = {
      getHandler: () => ({}),
      getClass: () => ({}),
    } as ExecutionContext;
    expect(guard.canActivate(context)).toBe(true);
  });
});
