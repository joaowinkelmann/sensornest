import { Controller, Get, Res } from '@nestjs/common';
import { AppService } from './app.service';
import { Public } from './auth/public.decorator';
import { ApiOperation } from '@nestjs/swagger';

@Controller()
export class AppController {
  constructor(private readonly appService: AppService) {}

  @Public()
  @Get()
  @ApiOperation({
    summary: 'Redireciona para a documentação da API. (Essa página)',
  })
  getHello(@Res() res) {
    // Agora redireciona para a documentação em /docs
    res.status(302).redirect('/sensornest/api/docs');
  }

  // @Public()
  // @Get('/sendmailtest')
  // async sendMailer(@Res() response: any) {
  //   const mail = await this.appService.sendMail();

  //   return response.status(200).send({
  //     message: 'success',
  //     mail,
  //   });
  // }
}
