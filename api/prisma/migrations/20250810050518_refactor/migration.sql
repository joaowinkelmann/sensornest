/*
  Warnings:

  - You are about to drop the column `created_at` on the `controller` table. All the data in the column will be lost.
  - You are about to drop the column `lat` on the `controller` table. All the data in the column will be lost.
  - You are about to drop the column `long` on the `controller` table. All the data in the column will be lost.
  - You are about to drop the column `mesh_id` on the `controller` table. All the data in the column will be lost.
  - You are about to drop the column `updated_at` on the `controller` table. All the data in the column will be lost.
  - You are about to drop the column `createdAt` on the `mesh` table. All the data in the column will be lost.
  - You are about to drop the column `updatedAt` on the `mesh` table. All the data in the column will be lost.
  - You are about to drop the column `created_at` on the `reading` table. All the data in the column will be lost.
  - You are about to drop the column `updated_at` on the `reading` table. All the data in the column will be lost.
  - You are about to drop the column `createdAt` on the `user` table. All the data in the column will be lost.
  - You are about to drop the column `updatedAt` on the `user` table. All the data in the column will be lost.
  - You are about to drop the column `created_at` on the `worker` table. All the data in the column will be lost.
  - You are about to drop the column `lat` on the `worker` table. All the data in the column will be lost.
  - You are about to drop the column `long` on the `worker` table. All the data in the column will be lost.
  - You are about to drop the column `mesh_id` on the `worker` table. All the data in the column will be lost.
  - You are about to drop the column `updated_at` on the `worker` table. All the data in the column will be lost.
  - Added the required column `alt_dthr` to the `controller` table without a default value. This is not possible if the table is not empty.
  - Added the required column `x` to the `controller` table without a default value. This is not possible if the table is not empty.
  - Added the required column `y` to the `controller` table without a default value. This is not possible if the table is not empty.
  - Added the required column `alt_dthr` to the `mesh` table without a default value. This is not possible if the table is not empty.
  - Added the required column `alt_dthr` to the `reading` table without a default value. This is not possible if the table is not empty.
  - Added the required column `alt_dthr` to the `user` table without a default value. This is not possible if the table is not empty.
  - Added the required column `alt_dthr` to the `worker` table without a default value. This is not possible if the table is not empty.
  - Added the required column `x` to the `worker` table without a default value. This is not possible if the table is not empty.
  - Added the required column `y` to the `worker` table without a default value. This is not possible if the table is not empty.

*/
-- CreateEnum
CREATE TYPE "public"."DeviceStatus" AS ENUM ('PENDING', 'ACTIVE', 'DISABLED');

-- DropForeignKey
ALTER TABLE "public"."controller" DROP CONSTRAINT "controller_mesh_id_fkey";

-- DropForeignKey
ALTER TABLE "public"."mesh" DROP CONSTRAINT "mesh_user_id_fkey";

-- DropForeignKey
ALTER TABLE "public"."reading" DROP CONSTRAINT "reading_workerId_fkey";

-- DropForeignKey
ALTER TABLE "public"."worker" DROP CONSTRAINT "worker_mesh_id_fkey";

-- AlterTable
ALTER TABLE "public"."controller" DROP COLUMN "created_at",
DROP COLUMN "lat",
DROP COLUMN "long",
DROP COLUMN "mesh_id",
DROP COLUMN "updated_at",
ADD COLUMN     "alt_dthr" TIMESTAMP(3) NOT NULL,
ADD COLUMN     "ins_dthr" TIMESTAMP(3) NOT NULL DEFAULT CURRENT_TIMESTAMP,
ADD COLUMN     "status" "public"."DeviceStatus" NOT NULL DEFAULT 'PENDING',
ADD COLUMN     "x" DOUBLE PRECISION NOT NULL,
ADD COLUMN     "y" DOUBLE PRECISION NOT NULL,
ADD COLUMN     "zone_id" TEXT;

-- AlterTable
ALTER TABLE "public"."mesh" DROP COLUMN "createdAt",
DROP COLUMN "updatedAt",
ADD COLUMN     "alt_dthr" TIMESTAMP(3) NOT NULL,
ADD COLUMN     "ins_dthr" TIMESTAMP(3) NOT NULL DEFAULT CURRENT_TIMESTAMP;

-- AlterTable
ALTER TABLE "public"."reading" DROP COLUMN "created_at",
DROP COLUMN "updated_at",
ADD COLUMN     "alt_dthr" TIMESTAMP(3) NOT NULL,
ADD COLUMN     "controllerId" TEXT,
ADD COLUMN     "ins_dthr" TIMESTAMP(3) NOT NULL DEFAULT CURRENT_TIMESTAMP,
ALTER COLUMN "temperature" DROP NOT NULL,
ALTER COLUMN "humidity" DROP NOT NULL,
ALTER COLUMN "workerId" DROP NOT NULL;

-- AlterTable
ALTER TABLE "public"."user" DROP COLUMN "createdAt",
DROP COLUMN "updatedAt",
ADD COLUMN     "alt_dthr" TIMESTAMP(3) NOT NULL,
ADD COLUMN     "ins_dthr" TIMESTAMP(3) NOT NULL DEFAULT CURRENT_TIMESTAMP;

-- AlterTable
ALTER TABLE "public"."worker" DROP COLUMN "created_at",
DROP COLUMN "lat",
DROP COLUMN "long",
DROP COLUMN "mesh_id",
DROP COLUMN "updated_at",
ADD COLUMN     "alt_dthr" TIMESTAMP(3) NOT NULL,
ADD COLUMN     "ins_dthr" TIMESTAMP(3) NOT NULL DEFAULT CURRENT_TIMESTAMP,
ADD COLUMN     "status" "public"."DeviceStatus" NOT NULL DEFAULT 'PENDING',
ADD COLUMN     "x" DOUBLE PRECISION NOT NULL,
ADD COLUMN     "y" DOUBLE PRECISION NOT NULL,
ADD COLUMN     "zone_id" TEXT;

-- CreateTable
CREATE TABLE "public"."zone" (
    "id" TEXT NOT NULL,
    "name" TEXT NOT NULL,
    "ins_dthr" TIMESTAMP(3) NOT NULL DEFAULT CURRENT_TIMESTAMP,
    "alt_dthr" TIMESTAMP(3) NOT NULL,
    "vertices" JSONB NOT NULL,
    "mesh_id" TEXT NOT NULL,

    CONSTRAINT "zone_pkey" PRIMARY KEY ("id")
);

-- AddForeignKey
ALTER TABLE "public"."mesh" ADD CONSTRAINT "mesh_user_id_fkey" FOREIGN KEY ("user_id") REFERENCES "public"."user"("id") ON DELETE CASCADE ON UPDATE CASCADE;

-- AddForeignKey
ALTER TABLE "public"."zone" ADD CONSTRAINT "zone_mesh_id_fkey" FOREIGN KEY ("mesh_id") REFERENCES "public"."mesh"("id") ON DELETE CASCADE ON UPDATE CASCADE;

-- AddForeignKey
ALTER TABLE "public"."controller" ADD CONSTRAINT "controller_zone_id_fkey" FOREIGN KEY ("zone_id") REFERENCES "public"."zone"("id") ON DELETE SET NULL ON UPDATE CASCADE;

-- AddForeignKey
ALTER TABLE "public"."worker" ADD CONSTRAINT "worker_zone_id_fkey" FOREIGN KEY ("zone_id") REFERENCES "public"."zone"("id") ON DELETE SET NULL ON UPDATE CASCADE;

-- AddForeignKey
ALTER TABLE "public"."reading" ADD CONSTRAINT "reading_workerId_fkey" FOREIGN KEY ("workerId") REFERENCES "public"."worker"("id") ON DELETE CASCADE ON UPDATE CASCADE;

-- AddForeignKey
ALTER TABLE "public"."reading" ADD CONSTRAINT "reading_controllerId_fkey" FOREIGN KEY ("controllerId") REFERENCES "public"."controller"("id") ON DELETE CASCADE ON UPDATE CASCADE;
