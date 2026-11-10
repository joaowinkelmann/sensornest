/*
  Warnings:

  - You are about to drop the `controllers` table. If the table is not empty, all the data it contains will be lost.
  - You are about to drop the `meshes` table. If the table is not empty, all the data it contains will be lost.
  - You are about to drop the `sensor_readings` table. If the table is not empty, all the data it contains will be lost.
  - You are about to drop the `sensors` table. If the table is not empty, all the data it contains will be lost.
  - You are about to drop the `users` table. If the table is not empty, all the data it contains will be lost.

*/
-- DropForeignKey
ALTER TABLE "public"."controllers" DROP CONSTRAINT "controllers_mesh_id_fkey";

-- DropForeignKey
ALTER TABLE "public"."meshes" DROP CONSTRAINT "meshes_user_id_fkey";

-- DropForeignKey
ALTER TABLE "public"."sensor_readings" DROP CONSTRAINT "sensor_readings_workerId_fkey";

-- DropForeignKey
ALTER TABLE "public"."sensors" DROP CONSTRAINT "sensors_mesh_id_fkey";

-- DropTable
DROP TABLE "public"."controllers";

-- DropTable
DROP TABLE "public"."meshes";

-- DropTable
DROP TABLE "public"."sensor_readings";

-- DropTable
DROP TABLE "public"."sensors";

-- DropTable
DROP TABLE "public"."users";

-- CreateTable
CREATE TABLE "public"."user" (
    "id" TEXT NOT NULL,
    "email" TEXT NOT NULL,
    "name" TEXT,
    "createdAt" TIMESTAMP(3) NOT NULL DEFAULT CURRENT_TIMESTAMP,
    "updatedAt" TIMESTAMP(3) NOT NULL,
    "password_hash" TEXT NOT NULL,

    CONSTRAINT "user_pkey" PRIMARY KEY ("id")
);

-- CreateTable
CREATE TABLE "public"."mesh" (
    "id" TEXT NOT NULL,
    "name" TEXT NOT NULL,
    "createdAt" TIMESTAMP(3) NOT NULL DEFAULT CURRENT_TIMESTAMP,
    "updatedAt" TIMESTAMP(3) NOT NULL,
    "user_id" TEXT NOT NULL,

    CONSTRAINT "mesh_pkey" PRIMARY KEY ("id")
);

-- CreateTable
CREATE TABLE "public"."controller" (
    "id" TEXT NOT NULL,
    "created_at" TIMESTAMP(3) NOT NULL DEFAULT CURRENT_TIMESTAMP,
    "updated_at" TIMESTAMP(3) NOT NULL,
    "mac_address" TEXT NOT NULL,
    "name" TEXT NOT NULL,
    "lat" DOUBLE PRECISION NOT NULL,
    "long" DOUBLE PRECISION NOT NULL,
    "description" TEXT,
    "mesh_id" TEXT NOT NULL,

    CONSTRAINT "controller_pkey" PRIMARY KEY ("id")
);

-- CreateTable
CREATE TABLE "public"."worker" (
    "id" TEXT NOT NULL,
    "created_at" TIMESTAMP(3) NOT NULL DEFAULT CURRENT_TIMESTAMP,
    "updated_at" TIMESTAMP(3) NOT NULL,
    "mac_address" TEXT NOT NULL,
    "name" TEXT NOT NULL,
    "lat" DOUBLE PRECISION NOT NULL,
    "long" DOUBLE PRECISION NOT NULL,
    "description" TEXT,
    "mesh_id" TEXT NOT NULL,

    CONSTRAINT "worker_pkey" PRIMARY KEY ("id")
);

-- CreateTable
CREATE TABLE "public"."reading" (
    "id" TEXT NOT NULL,
    "created_at" TIMESTAMP(3) NOT NULL DEFAULT CURRENT_TIMESTAMP,
    "updated_at" TIMESTAMP(3) NOT NULL,
    "temperature" DOUBLE PRECISION NOT NULL,
    "humidity" DOUBLE PRECISION NOT NULL,
    "reading_time" TIMESTAMP(3) NOT NULL,
    "workerId" TEXT NOT NULL,

    CONSTRAINT "reading_pkey" PRIMARY KEY ("id")
);

-- CreateIndex
CREATE UNIQUE INDEX "user_email_key" ON "public"."user"("email");

-- CreateIndex
CREATE UNIQUE INDEX "controller_mac_address_key" ON "public"."controller"("mac_address");

-- CreateIndex
CREATE UNIQUE INDEX "worker_mac_address_key" ON "public"."worker"("mac_address");

-- AddForeignKey
ALTER TABLE "public"."mesh" ADD CONSTRAINT "mesh_user_id_fkey" FOREIGN KEY ("user_id") REFERENCES "public"."user"("id") ON DELETE RESTRICT ON UPDATE CASCADE;

-- AddForeignKey
ALTER TABLE "public"."controller" ADD CONSTRAINT "controller_mesh_id_fkey" FOREIGN KEY ("mesh_id") REFERENCES "public"."mesh"("id") ON DELETE RESTRICT ON UPDATE CASCADE;

-- AddForeignKey
ALTER TABLE "public"."worker" ADD CONSTRAINT "worker_mesh_id_fkey" FOREIGN KEY ("mesh_id") REFERENCES "public"."mesh"("id") ON DELETE RESTRICT ON UPDATE CASCADE;

-- AddForeignKey
ALTER TABLE "public"."reading" ADD CONSTRAINT "reading_workerId_fkey" FOREIGN KEY ("workerId") REFERENCES "public"."worker"("id") ON DELETE RESTRICT ON UPDATE CASCADE;
