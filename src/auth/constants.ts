export const jwtConstants = {
  get secret() {
    return process.env.JWT_SECRET;
  },
  // ensure expiresIn is always a string no matter what
  expiresIn: process.env.JWT_EXPIRES_IN ?? '86400s',
};
