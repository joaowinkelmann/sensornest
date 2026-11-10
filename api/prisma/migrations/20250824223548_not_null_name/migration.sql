/*
  Warnings:

  - Made the column `name` on table `user` required. This step will fail if there are existing NULL values in that column.

*/
-- AlterTable
ALTER TABLE "public"."user" ALTER COLUMN "name" SET NOT NULL,
ALTER COLUMN "name" SET DEFAULT 'User';
