-- DropForeignKey
ALTER TABLE "public"."device" DROP CONSTRAINT "device_mesh_id_fkey";

-- AlterTable
ALTER TABLE "public"."device" ALTER COLUMN "mesh_id" DROP NOT NULL;

-- AddForeignKey
ALTER TABLE "public"."device" ADD CONSTRAINT "device_mesh_id_fkey" FOREIGN KEY ("mesh_id") REFERENCES "public"."mesh"("id") ON DELETE SET NULL ON UPDATE CASCADE;
