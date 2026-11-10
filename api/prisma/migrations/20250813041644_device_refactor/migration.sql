/*
  Warnings:

  - You are about to drop the column `controllerId` on the `reading` table. All the data in the column will be lost.
  - You are about to drop the column `workerId` on the `reading` table. All the data in the column will be lost.
  - You are about to drop the `controller` table. If the table is not empty, all the data it contains will be lost.
  - You are about to drop the `worker` table. If the table is not empty, all the data it contains will be lost.

*/
-- CreateEnum
CREATE TYPE "public"."DeviceRole" AS ENUM ('WORKER', 'CONTROLLER');

-- DropForeignKey
ALTER TABLE "public"."controller" DROP CONSTRAINT "controller_zone_id_fkey";

-- DropForeignKey
ALTER TABLE "public"."reading" DROP CONSTRAINT "reading_controllerId_fkey";

-- DropForeignKey
ALTER TABLE "public"."reading" DROP CONSTRAINT "reading_workerId_fkey";

-- DropForeignKey
ALTER TABLE "public"."worker" DROP CONSTRAINT "worker_zone_id_fkey";

-- AlterTable
ALTER TABLE "public"."reading" DROP COLUMN "controllerId",
DROP COLUMN "workerId",
ADD COLUMN     "deviceId" TEXT;

-- AlterTable
ALTER TABLE "public"."zone" ADD COLUMN     "bg_color" TEXT;

-- DropTable
DROP TABLE "public"."controller";

-- DropTable
DROP TABLE "public"."worker";

-- CreateTable
CREATE TABLE "public"."device" (
    "id" TEXT NOT NULL,
    "ins_dthr" TIMESTAMP(3) NOT NULL DEFAULT CURRENT_TIMESTAMP,
    "alt_dthr" TIMESTAMP(3) NOT NULL,
    "mac_address" TEXT NOT NULL,
    "name" TEXT NOT NULL,
    "description" TEXT,
    "x" DOUBLE PRECISION NOT NULL,
    "y" DOUBLE PRECISION NOT NULL,
    "status" "public"."DeviceStatus" NOT NULL DEFAULT 'PENDING',
    "role" "public"."DeviceRole" NOT NULL DEFAULT 'WORKER',
    "zone_id" TEXT,
    "device_color" TEXT,

    CONSTRAINT "device_pkey" PRIMARY KEY ("id")
);

-- CreateIndex
CREATE UNIQUE INDEX "device_mac_address_key" ON "public"."device"("mac_address");

-- AddForeignKey
ALTER TABLE "public"."device" ADD CONSTRAINT "device_zone_id_fkey" FOREIGN KEY ("zone_id") REFERENCES "public"."zone"("id") ON DELETE SET NULL ON UPDATE CASCADE;

-- AddForeignKey
ALTER TABLE "public"."reading" ADD CONSTRAINT "reading_deviceId_fkey" FOREIGN KEY ("deviceId") REFERENCES "public"."device"("id") ON DELETE CASCADE ON UPDATE CASCADE;
