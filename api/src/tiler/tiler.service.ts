import { Injectable, Logger } from '@nestjs/common';
import * as fs from 'node:fs';
import * as os from 'node:os';
import * as path from 'node:path';
import * as crypto from 'node:crypto';
import sharp from 'sharp';

export interface TileResult {
  tmpDir: string;
  id: string;
  maxZoom: number;
  width: number;
  height: number;
  prefix: string;
  ext: 'png' | 'webp';
}

@Injectable()
export class TilerService {
  private readonly logger = new Logger(TilerService.name);

  async makeTilesFromImageBuffer(
    buffer: Buffer,
    meshId?: string,
  ): Promise<TileResult> {
    this.logger.debug(
      `Starting tile generation from image buffer: length=${buffer.length} meshId=${meshId}`,
    );


    // Remove a atualização baseada no meshId para evitar cache-miss ao alterar a imagem de fundo. Assim, garantimos uma URL diferente a cada atualização dos tiles.
    const id = crypto.randomUUID();

    const prefix = `custom-maps/${id}`;

    // Formato de saída configurável (padrão webp por ser menor), qualidade/esforço
    const envFmt = (process.env.MAP_TILE_FORMAT || 'webp').toLowerCase();
    const format: 'png' | 'webp' = envFmt === 'png' ? 'png' : 'webp';
    const quality = Number.isFinite(Number(process.env.MAP_TILE_QUALITY))
      ? Math.max(1, Math.min(100, Number(process.env.MAP_TILE_QUALITY)))
      : 80;
    const effort = Number.isFinite(Number(process.env.MAP_TILE_EFFORT))
      ? Math.max(0, Math.min(6, Number(process.env.MAP_TILE_EFFORT)))
      : 4;
    const ext: 'png' | 'webp' = format;

    // Normaliza a orientação
    const base = sharp(buffer, { limitInputPixels: false }).rotate();
    const baseMeta = await base.metadata();
    const width = baseMeta.width ?? 0;
    const height = baseMeta.height ?? 0;

    if (!width || !height) {
      throw new Error('Invalid image: missing dimensions');
    }

    // Tamanho do tile (padrão 256)
    const tileSize = Number.isFinite(Number(process.env.MAP_TILE_SIZE))
      ? Math.max(64, Math.min(1024, Number(process.env.MAP_TILE_SIZE)))
      : 256;

    // Calcula um maxZoom automático para que a imagem use o máximo de tiles possível no nível superior.
    const tilesXNative = Math.max(1, Math.ceil(width / tileSize));
    const tilesYNative = Math.max(1, Math.ceil(height / tileSize));
    const autoMaxZoom = Math.ceil(
      Math.log2(Math.max(tilesXNative, tilesYNative)),
    );

    // Sempre constrói até um max zoom fixo (inclusivo)
    const envMaxZoom = process.env.MAP_MAX_ZOOM;
    const maxZoom =
      envMaxZoom && Number.isFinite(Number(envMaxZoom))
        ? Math.max(0, Number(envMaxZoom))
        : Math.max(0, autoMaxZoom);

    const tmpDir = await fs.promises.mkdtemp(
      path.join(os.tmpdir(), `sensornest-tiles-${id}-`),
    );

    this.logger.debug(
      `Generating tiles up to maxZoom=${maxZoom} (levels 0..${maxZoom}), tileSize=${tileSize}`,
    );

    // Gera pirâmide: z = 0..maxZoom (z=maxZoom usa dimensões originais)
    for (let z = 0; z <= maxZoom; z++) {
      const scale = 1 / Math.pow(2, maxZoom - z);
      const targetW = Math.max(1, Math.round(width * scale));
      const targetH = Math.max(1, Math.round(height * scale));

      // Redimensiona uma vez a partir da base normalizada; preserva AR
      const resizedBuf = await base
        .clone()
        .resize(targetW, targetH, { fit: 'inside', withoutEnlargement: true })
        .png()
        .toBuffer();

      const rMeta = await sharp(resizedBuf).metadata();
      const rW = rMeta.width ?? targetW;
      const rH = rMeta.height ?? targetH;

      // Define o canvas mundial para este zoom (espaço pixel). Não alocamos o canvas inteiro;
      // calculamos a região mínima de tiles delimitadores e compomos nela para evitar buffers enormes.
      const worldTiles = 2 ** z;
      const canvasWorldW = worldTiles * tileSize;
      const canvasWorldH = worldTiles * tileSize;

      // Centraliza a imagem redimensionada no espaço pixel (não em unidades de tile) para evitar arredondamento de meio tile.
      const leftWorldPx = Math.floor((canvasWorldW - rW) / 2);
      const topWorldPx = Math.floor((canvasWorldH - rH) / 2);

      // Calcula o intervalo mínimo de tiles mundiais que intercepta a imagem
      const minX = Math.max(0, Math.floor(leftWorldPx / tileSize));
      const maxX = Math.min(
        worldTiles - 1,
        Math.ceil((leftWorldPx + rW) / tileSize) - 1,
      );
      const minY = Math.max(0, Math.floor(topWorldPx / tileSize));
      const maxY = Math.min(
        worldTiles - 1,
        Math.ceil((topWorldPx + rH) / tileSize) - 1,
      );

      const spanX = Math.max(0, maxX - minX + 1);
      const spanY = Math.max(0, maxY - minY + 1);

      if (spanX === 0 || spanY === 0) {
        continue; // nada visível neste zoom
      }

      // Constrói um pequeno canvas local cobrindo apenas a região de tiles interceptados
      const localCanvasW = spanX * tileSize;
      const localCanvasH = spanY * tileSize;

      // Posição da imagem dentro do canvas local
      const localLeft = leftWorldPx - minX * tileSize;
      const localTop = topWorldPx - minY * tileSize;

      const composedBuf = await sharp({
        create: {
          width: localCanvasW,
          height: localCanvasH,
          channels: 4,
          background: { r: 0, g: 0, b: 0, alpha: 0 },
        },
      })
        .composite([{ input: resizedBuf, left: localLeft, top: localTop }])
        .png()
        .toBuffer();

      for (let lx = 0; lx < spanX; lx++) {
        for (let ly = 0; ly < spanY; ly++) {
          const worldX = minX + lx;
          const worldY = minY + ly;

          let tileSharp = sharp(composedBuf).extract({
            left: lx * tileSize,
            top: ly * tileSize,
            width: tileSize,
            height: tileSize,
          });

          if (format === 'webp') {
            tileSharp = tileSharp.webp({ quality, effort });
          } else {
            tileSharp = tileSharp.png({ compressionLevel: 9 });
          }

          const outDir = path.join(tmpDir, `${z}`, `${worldX}`);
          await fs.promises.mkdir(outDir, { recursive: true });
          const outPath = path.join(outDir, `${worldY}.${ext}`);
          await tileSharp.toFile(outPath);
        }
      }
    }

    this.logger.log(
      `Tiles generated in ${tmpDir} (maxZoom=${maxZoom}, size=${width}x${height}, ext=${ext})`,
    );

    return { tmpDir, id, maxZoom, width, height, prefix, ext };
  }
}
