-- CreateTable
CREATE TABLE "public"."users" (
    "id" TEXT NOT NULL,
    "email" TEXT NOT NULL,
    "name" TEXT,
    "createdAt" TIMESTAMP(3) NOT NULL DEFAULT CURRENT_TIMESTAMP,
    "updatedAt" TIMESTAMP(3) NOT NULL,
    "password_hash" TEXT NOT NULL,

    CONSTRAINT "users_pkey" PRIMARY KEY ("id")
);

-- CreateTable
CREATE TABLE "public"."meshes" (
    "id" TEXT NOT NULL,
    "name" TEXT NOT NULL,
    "createdAt" TIMESTAMP(3) NOT NULL DEFAULT CURRENT_TIMESTAMP,
    "updatedAt" TIMESTAMP(3) NOT NULL,
    "user_id" TEXT NOT NULL,

    CONSTRAINT "meshes_pkey" PRIMARY KEY ("id")
);

-- CreateTable
CREATE TABLE "public"."controllers" (
    "id" TEXT NOT NULL,
    "created_at" TIMESTAMP(3) NOT NULL DEFAULT CURRENT_TIMESTAMP,
    "updated_at" TIMESTAMP(3) NOT NULL,
    "mac_address" TEXT NOT NULL,
    "name" TEXT NOT NULL,
    "lat" DOUBLE PRECISION NOT NULL,
    "long" DOUBLE PRECISION NOT NULL,
    "description" TEXT,
    "mesh_id" TEXT NOT NULL,

    CONSTRAINT "controllers_pkey" PRIMARY KEY ("id")
);

-- CreateTable
CREATE TABLE "public"."sensors" (
    "id" TEXT NOT NULL,
    "created_at" TIMESTAMP(3) NOT NULL DEFAULT CURRENT_TIMESTAMP,
    "updated_at" TIMESTAMP(3) NOT NULL,
    "mac_address" TEXT NOT NULL,
    "name" TEXT NOT NULL,
    "lat" DOUBLE PRECISION NOT NULL,
    "long" DOUBLE PRECISION NOT NULL,
    "description" TEXT,
    "mesh_id" TEXT NOT NULL,

    CONSTRAINT "sensors_pkey" PRIMARY KEY ("id")
);

-- CreateTable
CREATE TABLE "public"."sensor_readings" (
    "id" TEXT NOT NULL,
    "created_at" TIMESTAMP(3) NOT NULL DEFAULT CURRENT_TIMESTAMP,
    "updated_at" TIMESTAMP(3) NOT NULL,
    "temperature" DOUBLE PRECISION NOT NULL,
    "humidity" DOUBLE PRECISION NOT NULL,
    "reading_time" TIMESTAMP(3) NOT NULL,
    "workerId" TEXT NOT NULL,

    CONSTRAINT "sensor_readings_pkey" PRIMARY KEY ("id")
);

-- CreateIndex
CREATE UNIQUE INDEX "users_email_key" ON "public"."users"("email");

-- CreateIndex
CREATE UNIQUE INDEX "controllers_mac_address_key" ON "public"."controllers"("mac_address");

-- CreateIndex
CREATE UNIQUE INDEX "sensors_mac_address_key" ON "public"."sensors"("mac_address");

-- AddForeignKey
ALTER TABLE "public"."meshes" ADD CONSTRAINT "meshes_user_id_fkey" FOREIGN KEY ("user_id") REFERENCES "public"."users"("id") ON DELETE RESTRICT ON UPDATE CASCADE;

-- AddForeignKey
ALTER TABLE "public"."controllers" ADD CONSTRAINT "controllers_mesh_id_fkey" FOREIGN KEY ("mesh_id") REFERENCES "public"."meshes"("id") ON DELETE RESTRICT ON UPDATE CASCADE;

-- AddForeignKey
ALTER TABLE "public"."sensors" ADD CONSTRAINT "sensors_mesh_id_fkey" FOREIGN KEY ("mesh_id") REFERENCES "public"."meshes"("id") ON DELETE RESTRICT ON UPDATE CASCADE;

-- AddForeignKey
ALTER TABLE "public"."sensor_readings" ADD CONSTRAINT "sensor_readings_workerId_fkey" FOREIGN KEY ("workerId") REFERENCES "public"."sensors"("id") ON DELETE RESTRICT ON UPDATE CASCADE;
