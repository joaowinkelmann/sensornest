import { NestFactory } from '@nestjs/core';
import {
  FastifyAdapter,
  NestFastifyApplication,
} from '@nestjs/platform-fastify';
import { AppModule } from './app.module';
import { ValidationPipe } from '@nestjs/common';
import { DocumentBuilder, SwaggerModule } from '@nestjs/swagger';
import multipart from '@fastify/multipart';

async function bootstrap() {
  const app = await NestFactory.create<NestFastifyApplication>(
    AppModule,
    new FastifyAdapter(),
  );

  await app.register(multipart as any, {
    limits: {
      fileSize: 1024 * 1024 * 5, // 5mb - tamanho máximo do arquivo
      fieldNameSize: 100, // 100 bytes - tamanho máximo do nome do campo
      fields: 10, // 10 campos - número máximo de campos
      fieldSize: 100, // 100 bytes - tamanho máximo do valor do campo
      files: 5, // 5 arquivos - número máximo de arquivos
    },
  });

  app.useGlobalPipes(new ValidationPipe());

  app.setGlobalPrefix('sensornest/api/');

  const config = new DocumentBuilder()
    .setTitle('SensorNest API')
    .setDescription('API specification for SensorNest')
    .setVersion('v0.1')
    .setExternalDoc('OpenAPI JSON Format', '/sensornest/api/docs-json')
    .addServer('https://winkels.com.br')
    .setBasePath('/sensornest/api')
    .addBearerAuth()
    .build();
  const documentFactory = () => SwaggerModule.createDocument(app, config);
  // SwaggerModule.setup('docs', app, documentFactory);
  SwaggerModule.setup('sensornest/api/docs', app, documentFactory());

  await app.listen(process.env.APP_PORT || 3000, '0.0.0.0');
}
bootstrap();
