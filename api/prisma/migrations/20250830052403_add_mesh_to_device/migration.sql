/*
  Warnings:

  - Added the required column `mesh_id` to the `device` table without a default value. This is not possible if the table is not empty.

*/
-- AlterTable
ALTER TABLE "public"."device" ADD COLUMN "mesh_id" TEXT;

-- Update existing devices to link them to the first mesh of the same user.
-- This assumes every user with a device has at least one mesh.
-- If this assumption is not safe, you may need a more specific data migration strategy.
UPDATE "public"."device" d
SET "mesh_id" = (
    SELECT "id"
    FROM "public"."mesh" m
    WHERE m.user_id = d.user_id
    ORDER BY m.ins_dthr
    LIMIT 1
)
WHERE d.mesh_id IS NULL;

-- Now, alter the column to be NOT NULL, as all rows should be populated.
ALTER TABLE "public"."device" ALTER COLUMN "mesh_id" SET NOT NULL;

-- Finally, add the foreign key constraint.
ALTER TABLE "public"."device" ADD CONSTRAINT "device_mesh_id_fkey" FOREIGN KEY ("mesh_id") REFERENCES "public"."mesh"("id") ON DELETE CASCADE ON UPDATE CASCADE;
