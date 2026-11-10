/*
  Warnings:

  - You are about to drop the column `device_color` on the `device` table. All the data in the column will be lost.

*/
-- AlterTable
ALTER TABLE "public"."device" DROP COLUMN "device_color",
ADD COLUMN     "color" TEXT,
ADD COLUMN     "readings_per_batch" INTEGER,
ADD COLUMN     "wake_up_interval" INTEGER;
