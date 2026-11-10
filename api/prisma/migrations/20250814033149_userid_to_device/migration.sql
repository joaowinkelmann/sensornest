/*
  Warnings:

  - Added the required column `user_id` to the `device` table without a default value. This is not possible if the table is not empty.

*/
-- AlterTable
ALTER TABLE "public"."device" ADD COLUMN     "user_id" TEXT NOT NULL;

-- AddForeignKey
ALTER TABLE "public"."device" ADD CONSTRAINT "device_user_id_fkey" FOREIGN KEY ("user_id") REFERENCES "public"."user"("id") ON DELETE CASCADE ON UPDATE CASCADE;
