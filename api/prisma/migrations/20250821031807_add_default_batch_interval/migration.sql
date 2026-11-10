-- AlterTable
ALTER TABLE "public"."device" ALTER COLUMN "readings_per_batch" SET DEFAULT 1,
ALTER COLUMN "wake_up_interval" SET DEFAULT 3600;
