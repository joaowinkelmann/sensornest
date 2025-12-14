import {
  Injectable,
  NotFoundException,
  UnauthorizedException,
  Logger,
  BadRequestException,
} from '@nestjs/common';
import { PrismaService } from '../../prisma/prisma.service';
import { CreateDeviceDto } from './dto/create-device.dto';
import { UpdateDeviceDto } from './dto/update-device.dto';
// import { Device } from './entities/device.entity';
import { Device as PrismaDevice } from '@prisma/client';

@Injectable()
export class DevicesService {
  private readonly logger = new Logger(DevicesService.name);
  constructor(private readonly prisma: PrismaService) { }

  async create(
    createDeviceDto: CreateDeviceDto,
    userId: string,
  ): Promise<PrismaDevice> {
    this.logger.log(`Starting device creation for user ${userId}`);
    this.logger.debug(
      `Create device payload: ${JSON.stringify(createDeviceDto)}`,
    );

    // Valida campos obrigatórios
    if (!createDeviceDto.macAddress) {
      this.logger.error(
        `Device creation failed: Missing macAddress for user ${userId}`,
      );
      throw new BadRequestException('MAC address is required');
    }
    if (!createDeviceDto.name) {
      this.logger.error(
        `Device creation failed: Missing name for user ${userId}`,
      );
      throw new BadRequestException('Device name is required');
    }
    if (createDeviceDto.x === undefined || createDeviceDto.x === null) {
      this.logger.error(
        `Device creation failed: Missing x coordinate for user ${userId}`,
      );
      throw new BadRequestException('X coordinate is required');
    }
    if (createDeviceDto.y === undefined || createDeviceDto.y === null) {
      this.logger.error(
        `Device creation failed: Missing y coordinate for user ${userId}`,
      );
      throw new BadRequestException('Y coordinate is required');
    }

    try {
      // Verifica se um dispositivo com o mesmo MAC já existe
      const existingDevice = await this.prisma.device.findUnique({
        where: { macAddress: createDeviceDto.macAddress },
      });

      if (existingDevice) {
        this.logger.warn(
          `Device creation failed: MAC address ${createDeviceDto.macAddress} already exists (owned by user ${existingDevice.userId})`,
        );
        throw new BadRequestException(
          'Device with this MAC address already exists',
        );
      }

      const device = await this.prisma.device.create({
        data: {
          ...createDeviceDto,
          userId,
        },
      });

      this.logger.log(
        `Device created successfully: ${device.id} (${device.name}) for user ${userId}`,
      );
      return device;
    } catch (error) {
      this.logger.error(
        `Failed to create device for user ${userId}: ${error.message}`,
      );
      throw error;
    }
  }

  async findAll(): Promise<PrismaDevice[]> {
    this.logger.log('Fetching all devices');
    try {
      const devices = await this.prisma.device.findMany();
      this.logger.log(`Retrieved ${devices.length} devices`);
      return devices;
    } catch (error) {
      this.logger.error(`Failed to fetch all devices: ${error.message}`);
      throw error;
    }
  }

  async findOne(id: string): Promise<PrismaDevice | null> {
    this.logger.log(`Fetching device by ID: ${id}`);

    if (!id) {
      this.logger.error('Device lookup failed: Missing device ID');
      throw new BadRequestException('Device ID is required');
    }

    try {
      const device = await this.prisma.device.findUnique({
        where: { id },
      });

      if (device) {
        this.logger.log(`Device found: ${device.id} (${device.name})`);
      } else {
        this.logger.warn(`Device not found with ID: ${id}`);
      }

      return device;
    } catch (error) {
      this.logger.error(`Failed to fetch device ${id}: ${error.message}`);
      throw error;
    }
  }

  async findDevicesByZone(
    zoneId: string,
    userId: string,
  ): Promise<PrismaDevice[]> {
    this.logger.log(`Fetching devices for zone ${zoneId} by user ${userId}`);

    if (!zoneId) {
      this.logger.error(
        `Zone devices lookup failed: Missing zone ID for user ${userId}`,
      );
      throw new BadRequestException('Zone ID is required');
    }
    if (!userId) {
      this.logger.error('Zone devices lookup failed: Missing user ID');
      throw new BadRequestException('User ID is required');
    }

    try {
      const devices = await this.prisma.device.findMany({
        where: {
          zoneId,
          zone: { mesh: { userId } },
        },
      });

      this.logger.log(
        `Found ${devices.length} devices in zone ${zoneId} for user ${userId}`,
      );
      return devices;
    } catch (error) {
      this.logger.error(
        `Failed to fetch devices for zone ${zoneId} and user ${userId}: ${error.message}`,
      );
      throw error;
    }
  }

  async findDevicesByMesh(
    meshId: string,
    userId: string,
  ): Promise<PrismaDevice[]> {
    this.logger.log(`Fetching devices for mesh ${meshId} by user ${userId}`);

    if (!meshId) {
      this.logger.error(
        `Mesh devices lookup failed: Missing mesh ID for user ${userId}`,
      );
      throw new BadRequestException('Mesh ID is required');
    }
    if (!userId) {
      this.logger.error('Mesh devices lookup failed: Missing user ID');
      throw new BadRequestException('User ID is required');
    }

    try {
      const devices = await this.prisma.device.findMany({
        where: {
          mesh: { id: meshId, userId },
        },
      });

      this.logger.log(
        `Found ${devices.length} devices in mesh ${meshId} for user ${userId}`,
      );
      return devices;
    } catch (error) {
      this.logger.error(
        `Failed to fetch devices for mesh ${meshId} and user ${userId}: ${error.message}`,
      );
      throw error;
    }
  }

  async findDevicesByUser(userId: string): Promise<PrismaDevice[]> {
    this.logger.log(`Fetching all devices for user ${userId}`);

    if (!userId) {
      this.logger.error('User devices lookup failed: Missing user ID');
      throw new BadRequestException('User ID is required');
    }

    try {
      const devices = await this.prisma.device.findMany({
        where: { userId },
      });

      this.logger.log(`Found ${devices.length} devices for user ${userId}`);
      return devices;
    } catch (error) {
      this.logger.error(
        `Failed to fetch devices for user ${userId}: ${error.message}`,
      );
      throw error;
    }
  }

  async findByMacAndUser(
    macAddress: string,
    userId: string,
  ): Promise<PrismaDevice | false> {
    this.logger.log(
      `Searching for device with MAC ${macAddress} for user ${userId}`,
    );

    if (!macAddress) {
      this.logger.error(
        `MAC device lookup failed: Missing MAC address for user ${userId}`,
      );
      throw new BadRequestException('MAC address is required');
    }
    if (!userId) {
      this.logger.error(
        `MAC device lookup failed: Missing user ID for MAC ${macAddress}`,
      );
      throw new BadRequestException('User ID is required');
    }

    try {
      const device = await this.prisma.device.findFirst({
        where: { macAddress, userId },
      });

      if (!device) {
        this.logger.warn(
          `Device not found with MAC ${macAddress} for user ${userId}`,
        );
        return false;
      }

      this.logger.log(
        `Device found: ${device.id} (${device.name}) with MAC ${macAddress} for user ${userId}`,
      );
      return device;
    } catch (error) {
      this.logger.error(
        `Failed to find device with MAC ${macAddress} for user ${userId}: ${error.message}`,
      );
      if (error instanceof NotFoundException) {
        return false;
      }
      throw error;
    }
  }

  async update(
    id: string,
    updateDeviceDto: UpdateDeviceDto,
    userId: string,
  ): Promise<PrismaDevice> {
    this.logger.log(`Starting update for device ${id} by user ${userId}`);
    this.logger.debug(
      `Update device payload: ${JSON.stringify(updateDeviceDto)}`,
    );

    if (!id) {
      this.logger.error(
        `Device update failed: Missing device ID for user ${userId}`,
      );
      throw new BadRequestException('Device ID is required');
    }
    if (!userId) {
      this.logger.error(
        `Device update failed: Missing user ID for device ${id}`,
      );
      throw new BadRequestException('User ID is required');
    }
    if (!updateDeviceDto || Object.keys(updateDeviceDto).length === 0) {
      this.logger.error(
        `Device update failed: Empty update payload for device ${id} by user ${userId}`,
      );
      throw new BadRequestException('Update data is required');
    }

    try {
      const device = await this.prisma.device.findUnique({ where: { id } });

      if (!device) {
        this.logger.warn(
          `Device update failed: Device ${id} not found for user ${userId}`,
        );
        throw new NotFoundException('Device not found');
      }

      if (device.userId !== userId) {
        this.logger.warn(
          `User ${userId} attempted to update device ${id} without permission. Device owner: ${device.userId}`,
        );
        throw new UnauthorizedException(
          'You do not have permission to update this device',
        );
      }

      this.logger.log(
        `Updating device ${id} (${device.name}) for authorized user ${userId}`,
      );

      const updatedDevice = await this.prisma.device.update({
        where: { id },
        data: updateDeviceDto,
      });

      this.logger.log(`Device ${id} updated successfully by user ${userId}`);
      return updatedDevice;
    } catch (error) {
      if (
        error instanceof NotFoundException ||
        error instanceof UnauthorizedException
      ) {
        throw error;
      }
      this.logger.error(
        `Failed to update device ${id} for user ${userId}: ${error.message}`,
      );
      throw error;
    }
  }

  async remove(id: string, userId: string): Promise<PrismaDevice> {
    this.logger.log(`Starting deletion of device ${id} by user ${userId}`);

    if (!id) {
      this.logger.error(
        `Device deletion failed: Missing device ID for user ${userId}`,
      );
      throw new BadRequestException('Device ID is required');
    }
    if (!userId) {
      this.logger.error(
        `Device deletion failed: Missing user ID for device ${id}`,
      );
      throw new BadRequestException('User ID is required');
    }

    try {
      const device = await this.prisma.device.findUnique({ where: { id } });

      if (!device || device.userId !== userId) {
        this.logger.warn(
          `Device deletion failed: Device ${id} not found or access denied for user ${userId}`,
        );
        throw new Error('Device not found or access denied');
      }

      this.logger.log(
        `Deleting device ${id} (${device.name}) for authorized user ${userId}`,
      );

      const deletedDevice = await this.prisma.device.delete({
        where: { id },
      });

      this.logger.log(`Device ${id} deleted successfully by user ${userId}`);
      return deletedDevice;
    } catch (error) {
      this.logger.error(
        `Failed to delete device ${id} for user ${userId}: ${error.message}`,
      );
      throw error;
    }
  }
}
