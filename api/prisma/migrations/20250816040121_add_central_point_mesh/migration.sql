/*
  Warnings:

  - Added the required column `lat` to the `mesh` table without a default value. This is not possible if the table is not empty.
  - Added the required column `lon` to the `mesh` table without a default value. This is not possible if the table is not empty.

*/
-- AlterTable
ALTER TABLE "public"."mesh" ADD COLUMN     "lat" DOUBLE PRECISION NOT NULL,
ADD COLUMN     "lon" DOUBLE PRECISION NOT NULL,
ADD COLUMN     "map_url" TEXT;
