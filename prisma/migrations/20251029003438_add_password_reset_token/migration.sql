/*
  Warnings:

  - A unique constraint covering the columns `[password_reset_token]` on the table `user` will be added. If there are existing duplicate values, this will fail.

*/
-- AlterTable
ALTER TABLE "user" ADD COLUMN     "password_reset_expires" TIMESTAMP(3),
ADD COLUMN     "password_reset_token" TEXT;

-- CreateIndex
CREATE UNIQUE INDEX "user_password_reset_token_key" ON "user"("password_reset_token");
