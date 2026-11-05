[English](../../README.md) | [Türkçe](README.tr.md) | [简体中文](README.zh-CN.md) | [Español](README.es.md) | **Português** | [Русский](README.ru.md) | [日本語](README.ja.md) | [Deutsch](README.de.md) | [Français](README.fr.md) | [한국어](README.ko.md) | [Bahasa Indonesia](README.id.md)

# OpenLiteSpeed for Windows

Site: [openlitespeed.dev/windows](https://openlitespeed.dev/windows/)

OpenLiteSpeed para Windows é uma compilação nativa para Windows do servidor web [OpenLiteSpeed](https://openlitespeed.org) 1.9.4. Ele roda no Windows 10, no Windows 11 e no Windows Server sem WSL, Docker ou máquina virtual, executa PHP via LSAPI ou FastCGI e pode ser instalado sozinho ou dentro do Laragon e do XAMPP. A edição full stack adiciona um painel, MariaDB, phpMyAdmin e Cloudflare Tunnel, para que um site no seu computador possa ficar online mesmo atrás de um IP local. Pelo painel você instala versões do PHP e do Node.js, define uma versão do PHP para cada site, cria sites WordPress, Laravel, Node.js e Python e gerencia os bancos de dados. Gratuito e de código aberto sob a licença GPL-3.0.

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../panel-dark.png">
  <img src="../panel-light.png" width="880" alt="Visão geral do painel do OpenLiteSpeed para Windows com o servidor web, MariaDB, Cloudflare Tunnel, o número de requisições em tempo real e os sites locais">
</picture>

## Recursos

- Programa Win32 nativo compilado com Visual C++, com instalador gráfico, aplicativo na bandeja do sistema e serviço do Windows
- Duas edições em um só instalador: apenas o servidor web, ou a versão full stack com painel, MariaDB, phpMyAdmin e Cloudflare Tunnel
- Painel: uma única janela para iniciar e parar os serviços, criar sites, instalar PHP e Node.js, gerenciar bancos de dados, ler os logs, alterar portas e colocar sites online
- PHP 7.4 a 8.5 e versões LTS do Node.js instaladas pelo painel e verificadas com o SHA-256 publicado
- Uma versão do PHP para cada site, extensões do PHP ativadas e desativadas com uma chave e as configurações mais comuns do `php.ini` em um formulário
- Novos sites como site PHP simples, WordPress, Laravel (com Composer), aplicativo Node.js ou aplicativo Python
- Bancos de dados: criar um banco com o próprio usuário, exportar e importar arquivos `.sql`, remover um banco
- Colocar um site local online com Cloudflare Tunnel: um endereço https público para um site no seu computador, sem redirecionamento de portas e sem IP público. Seu próprio domínio funciona com um token de túnel.
- MariaDB 11.8 e phpMyAdmin 5.2 somente em 127.0.0.1, com uma senha de root gerada automaticamente. Cada novo site WordPress recebe o próprio banco de dados e o próprio usuário.
- Usa todos os núcleos da CPU: até 32 processos worker HTTP nas mesmas portas, reiniciados automaticamente após uma falha, com recarga sem tempo de inatividade
- PHP 8.2, 8.3, 8.4 e 8.5 via LSAPI (o `lsphp.exe` para Windows está incluído) ou FastCGI (`php-cgi.exe`)
- Encontra as versões do PHP instaladas, o Laragon e o XAMPP e se configura para eles
- Importa sites do Apache a partir do `httpd.conf`: hosts virtuais, certificados SSL, redirecionamentos, aliases, proxies, CGI, regras de acesso e proteção por senha, incluindo as regras dos arquivos `.htaccess`
- Programas CGI: arquivos `.exe`, `.bat` e `.cmd` e scripts com uma linha `#!`, como Perl ou Python, cada um em seu próprio objeto de trabalho (job object) com um token restrito
- Aplicativos Node.js e Python (WSGI), iniciados e reiniciados pelo servidor. Aplicativos Node.js rodam sem alterações com o [openlitespeed-node](https://github.com/NeuralFoundry/openlitespeed-node), WebSockets incluídos, veja [docs/APPS.md](../APPS.md)
- Sites de desenvolvimento local: cada pasta dentro da pasta de projetos vira `http://folder.test/`. Novos sites, opcionalmente com WordPress, pela bandeja ou pelo painel
- HTTPS confiável para sites locais: uma autoridade certificadora criada no seu computador que só pode assinar nomes `.test`, `.localhost` e `.internal` e o próprio endereço do computador
- Sites proxy para aplicativos que rodam em uma porta local, como `api.test` para um aplicativo em `127.0.0.1:3000`
- Atualizações assinadas: o aplicativo da bandeja baixa uma nova versão, verifica a assinatura e a instala quando você confirma
- HTTP/2, HTTP/3 (QUIC), TLS 1.3, Brotli, LSCache e o console WebAdmin
- Certificados HTTPS gratuitos do Let's Encrypt, solicitados e renovados automaticamente por um cliente ACME integrado, veja [docs/ACME.md](../ACME.md)
- Módulos integrados: LSCache, progresso de upload, análise do corpo da requisição e scripts Lua com LuaJIT, veja [docs/MODULES.md](../MODULES.md)
- Arquivos `.htaccess` funcionam como no Apache: regras de reescrita, regras de acesso (`Require`, `Order`, `Allow`, `Deny`), senhas, seções `<Files>`, `Options Indexes` e `php_flag engine off`, aplicados em até um segundo quando um arquivo muda
- Firewall de aplicações web: ModSecurity 3 com o OWASP Core Rule Set, ativado com um único comando, veja [docs/PROTECTION.md](../PROTECTION.md#web-application-firewall)
- Proteção contra bots: desafios reCAPTCHA ou hCaptcha, limites de requisições, banda e conexões por IP de cliente, e o país do visitante a partir dos bancos de dados MaxMind GeoIP2 ou IP2Location para o PHP e as regras de reescrita, veja [docs/PROTECTION.md](../PROTECTION.md)
- O LiteSpeed Cache para WordPress funciona de imediato: instale e ative o plugin, sem nenhuma configuração no servidor. As páginas são limpas do cache automaticamente quando o conteúdo muda.
- Reforçado para o Windows: os nomes de arquivo são verificados contra truques com maiúsculas e minúsculas, nomes curtos, fluxos de dados e nomes de dispositivo, o PHP roda com um token restrito e o serviço roda com uma conta virtual

## Desempenho: OpenLiteSpeed para Windows vs. nginx e Apache

A compilação nativa para Windows deste repositório comparada ao nginx e ao Apache na mesma máquina com Windows 11, com PHP 8.4 e os mesmos 12 núcleos de CPU para cada servidor. Requisições por segundo, quanto maior, melhor. Todos os testes terminaram sem erros.

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../bench-windows-overview-dark.svg">
  <img src="../bench-windows-overview-light.svg" width="880" alt="OpenLiteSpeed para Windows vs. nginx e Apache: requisições por segundo para um arquivo estático, um aplicativo PHP e WordPress">
</picture>

| Servidor | Arquivo estático | Aplicativo PHP | WordPress, sem cache | PHP via HTTPS | PHP com 512 conexões |
|---|--:|--:|--:|--:|--:|
| OpenLiteSpeed + LSAPI | 175.203 | 9.548 | 117 | 8.687 | 9.434 |
| OpenLiteSpeed + FastCGI |  | 9.538 | 119 | 8.961 | 9.587 |
| nginx + PHP-CGI | 60.306 | 2.949 | 122 | 472 | 462 |
| Apache + mod_php | 17.889 | 7.532 | 105 | 4.473 | 3.078 |
| Apache + PHP-CGI |  | 5.743 | 118 | 4.356 | 5.396 |

Arquivos estáticos não envolvem o PHP, então foram medidos uma vez por servidor. Arquivo estático, aplicativo PHP e WordPress: testes de 20 segundos com 128 conexões keep-alive, 64 para o WordPress. HTTPS: 10 segundos, 128 conexões. A última coluna é a média de um teste de estresse contínuo de 60 segundos com 512 conexões:

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../bench-windows-stress-app-dark.svg">
  <img src="../bench-windows-stress-app-light.svg" width="880" alt="Teste de estresse do OpenLiteSpeed para Windows: requisições por segundo a cada 10 segundos durante 60 segundos com 512 conexões">
</picture>

O WordPress sem cache roda mais ou menos na mesma velocidade em todos os servidores, porque o PHP e o MySQL fazem a maior parte do trabalho. Com um cache de páginas, o cenário muda. A página inicial do mesmo site, 64 conexões:

| Servidor e cache | Requisições/s | p99 |
|---|--:|--:|
| OpenLiteSpeed + plugin LiteSpeed Cache | 21.180 | 6,1 ms |
| Apache + mod_cache_disk | 18.362 | 6,1 ms |
| nginx + fastcgi_cache | 7.745 | 10,8 ms |

Latência, memória, a configuração completa e os dados brutos estão em [docs/BENCHMARKS.md](../BENCHMARKS.md).

## Requisitos

- Windows 10, Windows 11 ou Windows Server 2019 ou mais recente, x64
- PHP 8.2 ou mais recente para Windows, x64. O instalador pode baixá-lo.

## Instalação

Baixe o `OpenLiteSpeed-1.9.4-win64-setup.exe` em [Releases](https://github.com/NeuralFoundry/openlitespeed-windows/releases/latest) e execute-o. O instalador mostra as versões do PHP que encontrou, sugere o tipo de instalação e pergunta qual edição instalar.

<img src="../setup.png" width="576" alt="Instalador do OpenLiteSpeed para Windows com a escolha da edição e a versão do PHP">

| Edição | Inclui |
|---|---|
| Full stack | OpenLiteSpeed, PHP, o aplicativo da bandeja, o painel, MariaDB, phpMyAdmin e Cloudflare Tunnel. O instalador baixa cerca de 170 MB e verifica cada arquivo com um SHA-256 fixado. |
| Somente servidor web | OpenLiteSpeed, PHP e o aplicativo da bandeja, com novos sites, HTTPS confiável, troca de versão do PHP, importações e atualizações |

Executar o instalador novamente troca a edição. Ao mudar para somente servidor web, seus bancos de dados são mantidos. A edição full stack roda com o aplicativo da bandeja, por isso a opção de serviço do Windows é apenas para a edição somente servidor web.

O instalador restringe a pasta de instalação à sua conta, aos administradores e ao sistema, para que outros usuários do computador não possam alterar os programas nem a configuração. Ele não altera as pastas do Laragon e do XAMPP, cujas pastas do PHP outros usuários muitas vezes podem alterar; com o serviço do Windows, isso permite que eles executem código com a conta do serviço, por isso a instalação do serviço avisa sobre essas pastas do PHP.

| Tipo de instalação | Pasta de instalação | Pasta de projetos | Iniciado por |
|---|---|---|---|
| Servidor independente | `C:\OpenLiteSpeed` | `C:\OpenLiteSpeed\www` | Aplicativo da bandeja ou serviço do Windows |
| Dentro do Laragon | `C:\laragon\bin\openlitespeed` | Raiz de documentos do Laragon | Laragon **Start All** |
| Dentro do XAMPP | `C:\xampp\openlitespeed` | `C:\xampp\htdocs` | Aplicativo da bandeja ou serviço do Windows |

O site responde na porta 80, ou na 8088 quando a porta 80 está em uso. O WebAdmin fica em `https://localhost:7080/`, com o usuário `admin` e a senha exibida no fim da instalação. Executar o instalador novamente sobre uma instalação existente faz a atualização e mantém a configuração.

<img src="../webadmin.png" width="880" alt="Console WebAdmin do OpenLiteSpeed rodando no Windows">

Instalação silenciosa:

```bat
OpenLiteSpeed-1.9.4-win64-setup.exe --silent --mode standalone --download-php 8.4 --edition full
```

| Opção | Descrição |
|---|---|
| `--mode standalone\|laragon\|xampp` | Tipo de instalação, detectado quando omitido |
| `--edition server\|full` | Edição; quando omitida, `server` para uma nova instalação e a atual para uma atualização. `--service` exige `server` |
| `--php <folder>` / `--download-php 8.2\|8.3\|8.4\|8.5` | PHP a ser usado |
| `--dir <folder>` / `--www <folder>` | Pasta de instalação / pasta de projetos |
| `--port <n>` / `--admin-port <n>` | Portas do site e do WebAdmin |
| `--service` / `--no-tray` | Instalar o serviço do Windows / não iniciar o aplicativo da bandeja |
| `--log <file>` | Arquivo de log, `%TEMP%\OpenLiteSpeed-setup.log` por padrão, inclui a senha de administrador |

Os mesmos arquivos estão disponíveis em um zip com `install.ps1` para implantações por script. Assim como o instalador, ele restringe a pasta de instalação à sua conta, aos administradores e ao sistema quando a pasta ainda herda as permissões da pasta acima:

```powershell
powershell -ExecutionPolicy Bypass -File install.ps1 -Mode standalone -DownloadPhp 8.4 -Edition full
```

Para desinstalar, use **Configurações > Aplicativos** ou execute `uninstall.exe --uninstall --silent` na pasta de instalação. A pasta de projetos e a pasta `data` com os bancos de dados são mantidas.

## Painel

Na edição full stack, o painel abre pelo menu Iniciar ou com um clique duplo no ícone da bandeja. É uma janela da área de trabalho, não uma página web, então não escuta em nenhuma porta. As páginas:

| Página | O que você pode fazer |
|---|---|
| Overview | Iniciar e parar o servidor web, o MariaDB e o Cloudflare Tunnel, ver as requisições por segundo e as conexões abertas |
| Sites | Criar e remover sites, escolher a versão do PHP ou do Node.js de cada site, colocar um site online, abrir a pasta dele, um terminal ou o VS Code nela, executar uma pasta como aplicativo Node.js ou Python, executar tarefas em segundo plano |
| PHP | Instalar versões do PHP, escolher a padrão, ativar e desativar extensões e o Xdebug, alterar configurações do `php.ini`, instalar o Composer |
| Node.js | Instalar versões do Node.js, escolher a que roda os aplicativos, reiniciar um aplicativo e ler o log dele |
| Databases | Criar um banco de dados e o usuário dele, exportar e importar arquivos `.sql`, remover um banco, abrir o phpMyAdmin |
| Mail | Ativar e desativar o capturador de e-mails, ler os e-mails capturados com a parte HTML e os anexos |
| Logs | Ler os logs do servidor, do MariaDB, dos aplicativos e do túnel, filtrar as linhas, acompanhar as novas linhas |
| Settings | Iniciar com o Windows, a pasta de projetos, HTTPS confiável, as portas HTTP e do WebAdmin e a senha do WebAdmin |

**Terminal**, no canto inferior esquerdo, abre um prompt de comando com o PHP padrão, o Composer, o Node.js e o cliente do MariaDB no `PATH`.

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../panel-sites-dark.png">
  <img src="../panel-sites-light.png" width="880" alt="Página Sites do painel com uma versão do PHP para cada site, aplicativos Node.js e Python, um site proxy e as chaves Online">
</picture>

O painel precisa do Microsoft Edge WebView2 Runtime, que já vem com o Windows 11 e com as versões atuais do Windows 10.

## Sites locais

Cada pasta dentro da pasta de projetos é um site em `http://folder.test/`. **New site** no painel ou na bandeja cria a pasta. O painel oferece estes tipos de site:

| Tipo | O que você recebe |
|---|---|
| Site PHP | Uma pasta com um `index.php` |
| WordPress | O WordPress mais recente. Quando o MariaDB está instalado, um banco de dados e um usuário para o site e um `wp-config.php` preenchido, de modo que a instalação do WordPress só pede o título do site e uma conta de administrador |
| Laravel | Um novo projeto Laravel criado com `composer create-project`, servido a partir da pasta `public` |
| Aplicativo Node.js | Um `server.js` e um `package.json` simples, iniciados pelo servidor com o Node.js padrão |
| Aplicativo Python | Um `wsgi.py` WSGI, iniciado pelo servidor com o Python encontrado no computador |

O instalador do WordPress só abre a partir deste computador, para que ninguém na rede consiga concluí-lo antes de você. Uma pasta com um `package.json` vira um aplicativo Node.js com **More > Run as a Node.js app**, e **Run as a PHP site** desfaz a mudança. **Remove site** move a pasta para a Lixeira e mantém os bancos de dados.

**New proxy site** dá um nome próprio a um aplicativo que escuta em uma porta local, por exemplo `app.test` para um servidor de desenvolvimento do Vite na porta 5173. As conexões WebSocket são repassadas. Um site proxy só responde a este computador, assim como o aplicativo por trás dele.

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../panel-site-dark.png">
  <img src="../panel-site-light.png" width="420" alt="Caixa de diálogo New site com o nome do site e os tipos de site: PHP, WordPress, Laravel, aplicativo Node.js e aplicativo Python">
</picture>
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../panel-proxy-dark.png">
  <img src="../panel-proxy-light.png" width="420" alt="Caixa de diálogo New proxy site com o nome do site e a porta do aplicativo">
</picture>

## Como colocar um site online

Ative **Online** ao lado de um site no painel ou execute `openlitespeed --tunnel start <site>`. O Cloudflare dá ao site um endereço aleatório `https://*.trycloudflare.com` que qualquer pessoa pode abrir até você desativá-lo. Funciona atrás de NAT, de um roteador ou de um IP local porque a conexão sai do seu computador, e nada é aberto no firewall. Os visitantes mantêm o próprio endereço IP nos logs e no PHP. O endereço público aparece abaixo do local, com **Copy** ao lado.

Para usar seu próprio domínio, crie um túnel no Cloudflare Zero Trust, aponte o hostname público dele para `http://127.0.0.1` na porta do site, com o nome do site como cabeçalho HTTP Host, e cole o token do túnel no painel em **Use your own domain**. O token é guardado criptografado para a sua conta do Windows.

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../panel-share-dark.png">
  <img src="../panel-share-light.png" width="420" alt="Caixa de diálogo para compartilhar um site local na internet pelo Cloudflare">
</picture>
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../panel-token-dark.png">
  <img src="../panel-token-light.png" width="420" alt="Caixa de diálogo para conectar seu próprio domínio com um token de túnel do Cloudflare">
</picture>

Só os sites da pasta de projetos e os sites proxy podem ficar online. Compartilhar um site proxy conecta o túnel diretamente ao aplicativo dele; com seu próprio domínio, aponte esse hostname também para a porta do aplicativo. O phpMyAdmin e o WebAdmin só respondem a este computador, e um túnel não inicia se o servidor não conseguir distinguir os visitantes deste computador.

## Bancos de dados

O MariaDB escuta somente em `127.0.0.1` e roda com a sua conta, nunca como administrador. O instalador baixa o MariaDB, e o aplicativo da bandeja cria os bancos de dados na primeira vez que é iniciado. A senha de root é gerada nesse momento e guardada em `data\client.cnf`, que só a sua conta e os administradores podem ler. `SELECT ... INTO OUTFILE` e `LOAD DATA` só funcionam em `data\files`. **Copy password** no painel ou na bandeja coloca a senha na área de transferência sem adicioná-la ao histórico da área de transferência. O phpMyAdmin fica em `http://phpmyadmin.test/`, somente para este computador.

Um site WordPress criado pela bandeja ou pelo painel recebe o próprio banco de dados e o próprio usuário, e o `wp-config.php` é preenchido.

Na página **Databases**, **New database** cria um banco de dados com um usuário de mesmo nome e uma senha gerada, exibida uma única vez com um botão **Copy**. **Export** grava um arquivo `.sql` com o `mariadb-dump`, **Import** carrega um arquivo `.sql` no banco de dados e **Remove** pede que você digite o nome antes e também pode remover o usuário.

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../panel-db-dark.png">
  <img src="../panel-db-light.png" width="880" alt="Página Databases do painel com o MariaDB, a senha de root e as opções de exportar, importar e remover para cada banco de dados">
</picture>

## HTTPS confiável

**Trusted HTTPS** no painel ou na bandeja, ou `openlitespeed --dev-https on`, cria uma autoridade certificadora no seu computador, pede ao Windows que confie nela para a sua conta e serve cada site local em `https://name.test/`. A autoridade é limitada por restrições de nome (name constraints) aos nomes `.test`, `.localhost` e `.internal` e a `127.0.0.1` e `::1`, então não consegue assinar um certificado para nenhum outro site. Ao desativar, ela é removida do repositório de confiança do Windows.

## PHP

O `lsphp.exe` é a SAPI do PHP para LiteSpeed compilada para Windows, para PHP 8.2 a 8.5 nas variantes thread safe e non thread safe. Ele roda sobre as compilações oficiais do PHP de [windows.php.net](https://windows.php.net), então as extensões existentes e as configurações do `php.ini` continuam funcionando. Quando uma versão do PHP não consegue carregá-lo, o OpenLiteSpeed passa a usar o `php-cgi.exe` via FastCGI.

<img src="../phpinfo.png" width="640" alt="phpinfo do PHP 8.4 no Windows com Server API LiteSpeed">

Troque a versão do PHP pelo painel, pelo menu da bandeja ou com `openlitespeed --set-php <folder>`.

**Install PHP**, na página PHP, lista as versões de windows.php.net, da 7.4 à 8.5, thread safe e non thread safe. Cada download é verificado com o SHA-256 que o windows.php.net publica, descompactado em `php\<version>` e recebe um `php.ini` com as extensões mais usadas ativadas. As versões a partir da 8.2 rodam via LSAPI, e as mais antigas via FastCGI.

Cada site pode usar a própria versão do PHP: escolha-a na coluna PHP da página Sites ou execute `openlitespeed --site php <site> <folder>`. Sites sem uma escolha seguem a versão padrão. Para cada versão, o menu **More** tem **Extensions**, com uma chave para cada extensão daquele PHP, e **Settings**, para o limite de memória, o tamanho de upload e de POST, o tempo de execução, as variáveis de entrada, a exibição de erros e o fuso horário. **Edit php.ini** abre o arquivo no Bloco de Notas. As alterações recarregam o servidor web.

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../panel-php-dark.png">
  <img src="../panel-php-light.png" width="880" alt="Página PHP do painel com três versões do PHP, a padrão, os sites que usam cada versão e o Composer">
</picture>

**Install**, em Composer, baixa o `composer.phar` de getcomposer.org, verificado com o SHA-256 dele. O terminal do painel tem o `composer` no `PATH`, e os sites Laravel são criados com ele.

## Xdebug

A chave na coluna Xdebug da página PHP, ou `openlitespeed --php xdebug <folder> on`, ativa o Xdebug para aquela versão do PHP. Se um Xdebug já for carregado da pasta `ext`, ele é usado. Caso contrário, a compilação oficial para Windows daquela versão do PHP é baixada de xdebug.org e verificada com um SHA-256 mantido no OpenLiteSpeed: Xdebug 3.5.3 para PHP 8.2 a 8.5, thread safe e non thread safe.

O Xdebug inicia a cada requisição e se conecta a um depurador em `127.0.0.1:9003`. Sem um depurador, a requisição continua imediatamente e nada é gravado nos logs. O JIT do opcache fica desativado enquanto o Xdebug está ativado e volta quando o Xdebug é desativado. As configurações ficam no fim do `php.ini`, e o servidor web é recarregado para aplicá-las.

O Xdebug não pode ser ativado enquanto o OpenLiteSpeed estiver instalado como serviço do Windows. Qualquer programa no computador que escute na porta 9003 recebe a sessão de depuração e pode executar código PHP com a conta do serviço, então desative o Xdebug antes de instalar o serviço.

**VS Code:** **More > Open in VS Code**, na página Sites, abre a pasta do site no VS Code quando ele está instalado. Inicie uma sessão de depuração PHP que escute na porta 9003, por exemplo com a extensão PHP Debug, e abra o site no navegador.

## Capturador de e-mails

A chave na página Mail, ou `openlitespeed --mail on`, captura todos os e-mails que os sites locais enviam, para que nada saia do computador. O `mail()` do PHP chega ao OpenLiteSpeed pelo `sendmail_path` no `php.ini` das versões do PHP que o OpenLiteSpeed executa, e os aplicativos que enviam por SMTP usam o host `127.0.0.1`, a porta 1025, sem criptografia e qualquer usuário e senha. Novos sites Laravel recebem essas configurações no `.env`. Uma conexão que começa como uma requisição web é fechada, para que uma página web no navegador não consiga enviar e-mails ao capturador.

Os e-mails ficam em `data\mail`, que só a sua conta pode ler, até as 1000 mensagens mais recentes e 512 MB. A página Mail mostra cada mensagem com as partes de texto e HTML e permite salvar os anexos. A parte HTML é exibida sem scripts e não carrega nada da internet.

## Tarefas em segundo plano

**Background jobs**, no menu **More** de um site, ou `openlitespeed --job add <site> <name> <command>`, executa um comando de longa duração como `php artisan queue:work`, `php artisan schedule:work` ou `npm run dev` na pasta do site, com as versões do PHP e do Node.js daquele site no início do `PATH`. As tarefas iniciam e param junto com o servidor web. Uma tarefa que termina é iniciada de novo após 1, 2, 4 e até 60 segundos, e depois de 10 reinícios em 10 minutos ela permanece parada. Parar uma tarefa encerra todos os processos que ela iniciou. A saída vai para `logs\jobs\<site>.<name>.log`, exibido na página Logs. Com o serviço do Windows, a conta do serviço executa as tarefas e o capturador de e-mails, assim como executa o PHP.

## Node.js

**Install Node.js**, na página Node.js, lista as versões Current e LTS de nodejs.org. Cada download é verificado com o `SHASUMS256.txt` e descompactado em `node\<version>`. A página também lista o Node.js encontrado em `Program Files`, no Laragon, no nvm, no Volta e no `PATH`. **Make default** escolhe a versão que roda os aplicativos Node.js, e os aplicativos aparecem abaixo com **Restart** e **Log**.

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../panel-node-dark.png">
  <img src="../panel-node-light.png" width="880" alt="Página Node.js do painel com as versões do Node.js instaladas e os aplicativos em execução">
</picture>

Um aplicativo Node.js não precisa de alterações: o `app.listen(3000)` é redirecionado para o endereço que o servidor fornece. Veja [docs/APPS.md](../APPS.md).

Cada site pode usar o próprio Node.js: escolha-o na coluna Version da página Sites ou execute `openlitespeed --site node <site> <node.exe>`. Sem uma escolha, um arquivo `.nvmrc` ou `.node-version` que indique uma versão principal como `20` seleciona o Node.js 20 mais recente instalado; caso contrário, o site roda com a versão padrão. As tarefas em segundo plano e o terminal do site usam a mesma versão.

## Migrando do Apache

O OpenLiteSpeed lê uma configuração existente do Apache e cria um site para cada `<VirtualHost>`, com os domínios, a raiz de documentos, o certificado SSL, os redirecionamentos, as regras de reescrita, os aliases, os proxies, as regras de acesso e a proteção por senha. As regras dos arquivos `.htaccess` continuam funcionando, porque o próprio OpenLiteSpeed as aplica.

```bat
bin\openlitespeed.exe --import-apache --dry-run
bin\openlitespeed.exe --import-apache C:\Apache24\conf\httpd.conf
bin\openlitespeed.exe -s reload
```

Sem um caminho, a configuração do Apache do Laragon, do XAMPP, do WampServer ou de `C:\Apache24` é encontrada automaticamente. `--dry-run` exibe o relatório sem alterar nada. Importar novamente substitui a importação anterior, e `--import-apache --remove` a remove. Regras que não podem ser convertidas são listadas no relatório. As regras de acesso são convertidas exatamente ou de forma mais restritiva, nunca mais permissiva; a única exceção, regras que bloqueiam clientes específicos por nome de host ou variável de ambiente, é informada como aviso. Os detalhes estão em [docs/APACHE.md](../APACHE.md).

## Migrando do nginx

`--import-nginx` faz o mesmo para o nginx: cada bloco `server` vira um site com os domínios, listeners, certificado SSL, locations, `try_files`, reescritas, redirecionamentos, aliases, proxies, regras de acesso e proteção por senha, e o PHP por trás do `fastcgi_pass` roda com o PHP do OpenLiteSpeed.

```bat
bin\openlitespeed.exe --import-nginx --dry-run
bin\openlitespeed.exe --import-nginx C:\nginx\conf\nginx.conf
```

Os detalhes estão em [docs/NGINX.md](../NGINX.md).

## Vários núcleos de CPU

O OpenLiteSpeed inicia um worker HTTP por núcleo de CPU, até 8 por padrão. Defina **Number of Workers** em **Server Configuration > General** no WebAdmin, ou `httpdWorkers` em `conf\httpd_config.conf`, com qualquer valor de 1 a 32. Cada worker tem o próprio pool de PHP. Veja em [docs/ARCHITECTURE.md](../ARCHITECTURE.md) como os workers compartilham os sockets de escuta no Windows.

## Linha de comando

Execute os comandos na pasta de instalação como `bin\openlitespeed.exe`.

| Comando | Descrição |
|---|---|
| `openlitespeed -s start\|stop\|restart\|reload\|status` | Controla o servidor, `reload` é um reinício sem tempo de inatividade |
| `openlitespeed -d` | Executa em primeiro plano |
| `openlitespeed --install-service` / `--uninstall-service` | Registra ou remove o serviço do Windows |
| `openlitespeed --set-php <folder>` | Usa outra versão do PHP |
| `openlitespeed --import-apache [httpd.conf] [--dry-run]` / `--import-apache --remove` | Importa os sites do Apache / remove os sites importados |
| `openlitespeed --import-nginx [nginx.conf] [--dry-run]` / `--import-nginx --remove` | Importa os sites do nginx / remove os sites importados |
| `openlitespeed --acme-install [--email <address>]` | Ativa os certificados automáticos do Let's Encrypt |
| `openlitespeed --modsec-install [--detection-only]` / `--modsec-install --remove` | Ativa ou desativa o firewall ModSecurity com o OWASP Core Rule Set |
| `openlitespeed --dev-sites <folder> [tld]` | Cria um site para cada subpasta de uma pasta |
| `openlitespeed --new-site <name> [--wordpress\|--laravel\|--node\|--python]` | Cria um site na pasta de projetos: um site PHP, WordPress com o banco de dados, um projeto Laravel, um aplicativo Node.js ou um aplicativo Python |
| `openlitespeed --site php <site> <folder>\|default` | Usa outra versão do PHP para um site |
| `openlitespeed --site app <site> node\|python\|off` | Executa a pasta de um site como aplicativo Node.js ou Python, ou de novo como site PHP |
| `openlitespeed --site node <site> <node.exe>\|default` | Usa outra versão do Node.js para um site |
| `openlitespeed --php list` | As versões do PHP neste computador em JSON, com os sites e o estado do Xdebug de cada uma |
| `openlitespeed --php available` / `install <8.x> [ts]` / `remove <folder>` | Lista e instala as versões do PHP de windows.php.net, remove uma versão baixada |
| `openlitespeed --php ext <folder>` / `ext-set <folder> <name> on\|off` | Lista as extensões de uma versão do PHP, ativa ou desativa uma delas |
| `openlitespeed --php ini <folder>` / `ini-set <folder> <key> <value>` | Lê ou altera uma configuração comum do `php.ini` |
| `openlitespeed --php xdebug <folder> on\|off\|status` | Ativa ou desativa o Xdebug para uma versão do PHP, ou mostra se está ativado |
| `openlitespeed --node list\|available` / `install <major>` / `default <node.exe>` / `remove <node.exe>` | Gerencia as versões do Node.js para os aplicativos |
| `openlitespeed --composer install` | Baixa o Composer em `tools\composer` |
| `openlitespeed --set-port http\|admin <port>` | Altera a porta do site ou do WebAdmin |
| `openlitespeed --proxy add <name> <port>` / `remove <name>` / `list` | Adiciona ou remove um site proxy para um aplicativo em uma porta local |
| `openlitespeed --dev-https on [port]\|off\|status` | HTTPS confiável para os sites locais |
| `openlitespeed --dev-hosts` / `--dev-hosts-clear` | Adiciona ou remove os nomes dos sites no arquivo hosts |
| `openlitespeed --db install\|fetch\|start\|stop\|status` | Instala, apenas baixa, inicia ou para o MariaDB e o phpMyAdmin |
| `openlitespeed --db list` / `create <name> [--no-user]` / `drop <name> [--user]` | Lista, cria ou remove bancos de dados |
| `openlitespeed --db export <name> <file.sql>` / `import <name> <file.sql>` | Exporta um banco de dados para um arquivo ou importa um arquivo para ele |
| `openlitespeed --tunnel start <site>` / `stop <site>` / `stop --all` / `status` | Coloca um site online com o Cloudflare Tunnel ou o tira do ar |
| `openlitespeed --tunnel token` / `run` / `run-stop` / `token clear` | Salva um token de túnel a partir da entrada padrão, conecta ou desconecta seu próprio domínio, esquece o token |
| `openlitespeed --mail on\|off\|status` | Ativa ou desativa o capturador de e-mails, ou mostra se ele está rodando |
| `openlitespeed --mail list` / `remove <id>` / `save <id> <n> <file>` / `clear` | Lista os e-mails capturados em JSON, remove uma mensagem, salva o anexo `n` (a partir de 0) de uma mensagem em um arquivo ou remove todas as mensagens |
| `openlitespeed --mail sendmail [-t] [-i] [-f <from>] [<recipient>...]` | Guarda uma mensagem vinda da entrada padrão, como o sendmail |
| `openlitespeed --job list` / `add <site> <name> <command>` / `remove <site> <name>` | Lista, adiciona ou remove as tarefas em segundo plano dos sites |
| `openlitespeed --job start\|stop\|restart <site> <name>` / `autostart <site> <name> on\|off` | Controla uma tarefa, escolhe se ela inicia com o servidor web |
| `openlitespeed --status` | Servidor, PHP, sites, banco de dados e túneis em JSON |
| `openlitespeed --admin-password admin\conf\htpasswd <password>` | Define a senha do WebAdmin |

O aplicativo da bandeja inicia, para e recarrega o servidor, abre os sites, o WebAdmin e os logs, cria sites e sites proxy, ativa e desativa o HTTPS confiável, troca a versão do PHP e importa sites do Apache e do nginx. Na edição full stack, ele também abre o painel e controla o MariaDB. Uma vez por dia, verifica no GitHub se há uma nova versão; **Update** baixa a versão, verifica a assinatura e o checksum e executa o novo instalador. `check=0` em `[update]` no `conf\olstray.ini` desativa a verificação.

## Compilando a partir do código-fonte

Veja [docs/BUILD.md](../BUILD.md). A compilação usa Visual Studio 2022, CMake e Ninja e baixa todas as dependências em commits fixados.

## Limitações

- Não há troca de usuário por host virtual (suEXEC). O PHP roda com o usuário que executa o servidor ou com a conta do serviço.
- Não há `SO_REUSEPORT`. Os workers compartilham um socket de escuta por endereço.
- cgroups, namespaces, bubblewrap e CageFS são recursos do Linux e não estão disponíveis.
- Aplicativos Ruby pelo contexto App Server não são suportados.

## Como contribuir

Relatos de bugs e pull requests são bem-vindos, veja [CONTRIBUTING.md](../../CONTRIBUTING.md). Informe problemas de segurança de forma privada, como descrito em [SECURITY.md](../../SECURITY.md).

## Licença

GPL-3.0, veja [LICENSE](../../LICENSE). O OpenLiteSpeed é desenvolvido pela LiteSpeed Technologies e licenciado sob a GPL-3.0. O `lsphp.exe` é compilado a partir do código-fonte do PHP e coberto pela [PHP License](../../win/lsphp/PHP-LICENSE.txt). Os componentes de terceiros estão listados em [THIRD-PARTY-NOTICES.txt](../../win/package/THIRD-PARTY-NOTICES.txt).

Este projeto não é afiliado nem endossado pela LiteSpeed Technologies ou pelo PHP Group. LiteSpeed e OpenLiteSpeed são marcas comerciais da LiteSpeed Technologies, Inc.

Do mesmo autor: [OpenLiteSpeed for cPanel and WHM](https://github.com/NeuralFoundry/openlitespeed-cpanel).
