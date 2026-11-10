/*
  Warnings:

  - Made the column `mesh_id` on table `device` required. This step will fail if there are existing NULL values in that column.

*/
-- AlterEnum
ALTER TYPE "public"."DeviceStatus" ADD VALUE 'INACTIVE';

-- DropForeignKey
ALTER TABLE "public"."device" DROP CONSTRAINT "device_mesh_id_fkey";

-- AlterTable
ALTER TABLE "public"."device" ALTER COLUMN "mesh_id" SET NOT NULL;

-- AddForeignKey
ALTER TABLE "public"."device" ADD CONSTRAINT "device_mesh_id_fkey" FOREIGN KEY ("mesh_id") REFERENCES "public"."mesh"("id") ON DELETE CASCADE ON UPDATE CASCADE;
