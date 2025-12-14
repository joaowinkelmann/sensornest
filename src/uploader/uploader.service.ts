import { Injectable, Logger } from '@nestjs/common';
import * as fs from 'node:fs';
import * as path from 'node:path';
import * as objectstorage from 'oci-objectstorage';
import * as common from 'oci-common';

@Injectable()
export class UploaderService {
  private readonly logger = new Logger(UploaderService.name);

  private readonly client: any;
  private readonly namespaceName: string;
  private readonly bucketName: string;
  private readonly region: string;

  constructor() {
    this.namespaceName = process.env.OCI_NAMESPACE ?? '';
    this.bucketName = process.env.OCI_BUCKET ?? '';
    this.region = process.env.OCI_REGION ?? '';

    const privateKey = process.env.OCI_PRIVATE_KEY_PATH
      ? fs.readFileSync(path.resolve(process.env.OCI_PRIVATE_KEY_PATH), 'utf-8')
      : (process.env.OCI_PRIVATE_KEY ?? '').replace(/\\n/g, '\n');

    const hasOciConfig =
      !!process.env.OCI_TENANCY &&
      !!process.env.OCI_USER &&
      !!process.env.OCI_FINGERPRINT &&
      (!!process.env.OCI_PRIVATE_KEY || !!process.env.OCI_PRIVATE_KEY_PATH) &&
      !!this.namespaceName &&
      !!this.bucketName &&
      !!this.region;

    if (!hasOciConfig) {
      this.logger.warn(
        'OCI configuration missing! UploaderService will run in NOOP mode.',
      );
      this.client = {
        putObject() {
          return { opcRequestId: 'noop' };
        },
      };
      return;
    }

    try {
      const provider = new common.SimpleAuthenticationDetailsProvider(
        process.env.OCI_TENANCY ?? '',
        process.env.OCI_USER ?? '',
        process.env.OCI_FINGERPRINT ?? '',
        privateKey,
        null, // senha
      );

      this.client = new objectstorage.ObjectStorageClient({
        authenticationDetailsProvider: provider,
      });

      if (this.region) {
        this.client.region = common.Region.fromRegionId(this.region as any);
      }
    } catch (e) {
      this.logger.warn(
        `Failed to initialize OCI client, switching to NOOP mode: ${String(e)}`,
      );
      this.client = {
        putObject() {
          return { opcRequestId: 'noop' };
        },
      };
    }
  }

  getBaseUrl(): string {
    // URL base pública para objetos
    return `https://objectstorage.${this.region}.oraclecloud.com/n/${this.namespaceName}/b/${this.bucketName}/o`;
  }

  getObjectUrl(key: string): string {
    return `${this.getBaseUrl()}/${encodeURIComponent(key)}`;
  }

  async uploadObject(
    key: string,
    body: Buffer,
    contentType = 'image/png',
  ): Promise<string> {
    this.logger.debug(`Uploading object to OCI: ${key} (${body.length} bytes)`);
    await this.client.putObject({
      namespaceName: this.namespaceName,
      bucketName: this.bucketName,
      objectName: key,
      putObjectBody: body,
      contentType,
    });
    return this.getObjectUrl(key);
  }

  async uploadDirectory(prefix: string, localDir: string): Promise<void> {
    const files = await this.walk(localDir);
    this.logger.log(
      `Uploading ${files.length} tiles to OCI under ${prefix}/...`,
    );

    for (const file of files) {
      const rel = path.relative(localDir, file).replace(/\\/g, '/');
      const key = `${prefix}/${rel}`;
      const body = await fs.promises.readFile(file);
      await this.uploadObject(key, body, 'image/png');
    }
  }

  private async walk(dir: string): Promise<string[]> {
    const entries = await fs.promises.readdir(dir, { withFileTypes: true });
    const files = await Promise.all(
      entries.map((e) => {
        const full = path.resolve(dir, e.name);
        return e.isDirectory() ? this.walk(full) : Promise.resolve([full]);
      }),
    );
    return files.flat();
  }
}
