[English](../../README.md) | [Türkçe](README.tr.md) | [简体中文](README.zh-CN.md) | **Español** | [Português](README.pt-BR.md) | [Русский](README.ru.md) | [日本語](README.ja.md) | [Deutsch](README.de.md) | [Français](README.fr.md) | [한국어](README.ko.md) | [Bahasa Indonesia](README.id.md)

# OpenLiteSpeed for Windows

Sitio web: [openlitespeed.dev/windows](https://openlitespeed.dev/windows/)

OpenLiteSpeed para Windows es una compilación nativa para Windows del servidor web [OpenLiteSpeed](https://openlitespeed.org) 1.9.4. Funciona en Windows 10, Windows 11 y Windows Server sin WSL, Docker ni máquina virtual, ejecuta PHP mediante LSAPI o FastCGI y se instala de forma independiente o dentro de Laragon y XAMPP. La edición full stack añade un panel de administración, MariaDB, phpMyAdmin y Cloudflare Tunnel, de modo que un sitio de su equipo puede publicarse en internet incluso detrás de una IP local. Desde el panel puede instalar versiones de PHP y Node.js, asignar a cada sitio su propia versión de PHP, crear sitios de WordPress, Laravel, Node.js y Python y administrar las bases de datos. Es gratuito y de código abierto con licencia GPL-3.0.

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../panel-dark.png">
  <img src="../panel-light.png" width="880" alt="Vista general del panel de OpenLiteSpeed para Windows con el servidor web, MariaDB, Cloudflare Tunnel, las solicitudes en tiempo real y los sitios locales">
</picture>

## Características

- Programa Win32 nativo compilado con Visual C++, con instalador gráfico, una aplicación en la bandeja del sistema y un servicio de Windows
- Dos ediciones en un solo instalador: solo el servidor web, o la versión full stack con panel, MariaDB, phpMyAdmin y Cloudflare Tunnel
- Panel: una sola ventana para iniciar y detener los servicios, crear sitios, instalar PHP y Node.js, administrar bases de datos, leer los registros, cambiar puertos y publicar sitios en internet
- PHP 7.4 a 8.5 y versiones LTS de Node.js instaladas desde el panel y verificadas con su SHA-256 publicado
- Una versión de PHP para cada sitio, extensiones de PHP que se activan y desactivan con un interruptor y los ajustes habituales de `php.ini` en un formulario
- Sitios nuevos como sitio PHP simple, WordPress, Laravel (con Composer), aplicación Node.js o aplicación Python
- Bases de datos: crear una base de datos con su propio usuario, exportar e importar archivos `.sql`, eliminar una base de datos
- Publicar un sitio local en internet con Cloudflare Tunnel: una dirección https pública para un sitio de su equipo, sin redirección de puertos ni IP pública. Su propio dominio funciona con un token de túnel.
- MariaDB 11.8 y phpMyAdmin 5.2 solo en 127.0.0.1, con una contraseña de root generada automáticamente. Cada sitio nuevo de WordPress recibe su propia base de datos y su propio usuario.
- Usa todos los núcleos de la CPU: hasta 32 procesos worker HTTP en los mismos puertos, reiniciados automáticamente tras un fallo, con recarga sin tiempo de inactividad
- PHP 8.2, 8.3, 8.4 y 8.5 mediante LSAPI (se incluye `lsphp.exe` para Windows) o FastCGI (`php-cgi.exe`)
- Detecta las versiones de PHP instaladas, Laragon y XAMPP y se configura para ellas
- Importa sitios de Apache desde `httpd.conf`: hosts virtuales, certificados SSL, redirecciones, alias, proxies, CGI, reglas de acceso y protección con contraseña, incluidas las reglas de los archivos `.htaccess`
- Programas CGI: archivos `.exe`, `.bat` y `.cmd` y scripts con una línea `#!`, como Perl o Python, cada uno en su propio objeto de trabajo (job object) con un token restringido
- Aplicaciones Node.js y Python (WSGI), iniciadas y reiniciadas por el servidor. Las aplicaciones Node.js funcionan sin cambios gracias a [openlitespeed-node](https://github.com/NeuralFoundry/openlitespeed-node), WebSockets incluidos, consulte [docs/APPS.md](../APPS.md)
- Sitios de desarrollo local: cada carpeta de la carpeta de proyectos se convierte en `http://folder.test/`. Sitios nuevos, opcionalmente con WordPress, desde la bandeja o el panel
- HTTPS de confianza para sitios locales: una autoridad de certificación creada en su equipo que solo puede firmar nombres `.test`, `.localhost` e `.internal` y la propia dirección del equipo
- Sitios proxy para aplicaciones que se ejecutan en un puerto local, como `api.test` para una aplicación en `127.0.0.1:3000`
- Actualizaciones firmadas: la aplicación de la bandeja descarga una nueva versión, comprueba su firma y la instala cuando usted lo confirma
- HTTP/2, HTTP/3 (QUIC), TLS 1.3, Brotli, LSCache y la consola WebAdmin
- Certificados HTTPS gratuitos de Let's Encrypt, solicitados y renovados automáticamente por un cliente ACME integrado, consulte [docs/ACME.md](../ACME.md)
- Módulos integrados: LSCache, progreso de subida, análisis del cuerpo de la solicitud y scripts Lua con LuaJIT, consulte [docs/MODULES.md](../MODULES.md)
- Los archivos `.htaccess` funcionan como en Apache: reglas de reescritura, reglas de acceso (`Require`, `Order`, `Allow`, `Deny`), contraseñas, secciones `<Files>`, `Options Indexes` y `php_flag engine off`, aplicados en menos de un segundo cuando cambia un archivo
- Firewall de aplicaciones web: ModSecurity 3 con OWASP Core Rule Set, activado con un solo comando, consulte [docs/PROTECTION.md](../PROTECTION.md#web-application-firewall)
- Protección contra bots: desafíos reCAPTCHA o hCaptcha, límites de solicitudes, ancho de banda y conexiones por IP de cliente, y el país del visitante a partir de las bases de datos MaxMind GeoIP2 o IP2Location para PHP y las reglas de reescritura, consulte [docs/PROTECTION.md](../PROTECTION.md)
- LiteSpeed Cache para WordPress funciona desde el primer momento: instale y active el plugin, sin configurar nada en el servidor. Las páginas se purgan automáticamente cuando cambia el contenido.
- Reforzado para Windows: los nombres de archivo se comprueban frente a trucos con mayúsculas y minúsculas, nombres cortos, flujos de datos y nombres de dispositivo, PHP se ejecuta con un token restringido y el servicio con una cuenta virtual

## Rendimiento: OpenLiteSpeed para Windows frente a nginx y Apache

La compilación nativa para Windows de este repositorio frente a nginx y Apache en el mismo equipo con Windows 11, con PHP 8.4 y los mismos 12 núcleos de CPU para cada servidor. Solicitudes por segundo, más es mejor. Todas las pruebas terminaron sin errores.

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../bench-windows-overview-dark.svg">
  <img src="../bench-windows-overview-light.svg" width="880" alt="OpenLiteSpeed para Windows frente a nginx y Apache: solicitudes por segundo para un archivo estático, una aplicación PHP y WordPress">
</picture>

| Servidor | Archivo estático | Aplicación PHP | WordPress, sin caché | PHP sobre HTTPS | PHP con 512 conexiones |
|---|--:|--:|--:|--:|--:|
| OpenLiteSpeed + LSAPI | 175.203 | 9.548 | 117 | 8.687 | 9.434 |
| OpenLiteSpeed + FastCGI |  | 9.538 | 119 | 8.961 | 9.587 |
| nginx + PHP-CGI | 60.306 | 2.949 | 122 | 472 | 462 |
| Apache + mod_php | 17.889 | 7.532 | 105 | 4.473 | 3.078 |
| Apache + PHP-CGI |  | 5.743 | 118 | 4.356 | 5.396 |

Los archivos estáticos no usan PHP, por eso se midieron una sola vez por servidor. Archivo estático, aplicación PHP y WordPress: pruebas de 20 segundos con 128 conexiones keep-alive, 64 para WordPress. HTTPS: 10 segundos, 128 conexiones. La última columna es la media de una prueba de carga continua de 60 segundos con 512 conexiones:

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../bench-windows-stress-app-dark.svg">
  <img src="../bench-windows-stress-app-light.svg" width="880" alt="Prueba de carga de OpenLiteSpeed para Windows: solicitudes por segundo cada 10 segundos durante 60 segundos con 512 conexiones">
</picture>

WordPress sin caché funciona a una velocidad parecida en todos los servidores, porque PHP y MySQL hacen la mayor parte del trabajo. Con una caché de páginas el panorama cambia. La página de inicio del mismo sitio, 64 conexiones:

| Servidor y caché | Solicitudes/s | p99 |
|---|--:|--:|
| OpenLiteSpeed + plugin LiteSpeed Cache | 21.180 | 6,1 ms |
| Apache + mod_cache_disk | 18.362 | 6,1 ms |
| nginx + fastcgi_cache | 7.745 | 10,8 ms |

La latencia, la memoria, la configuración completa y los datos sin procesar están en [docs/BENCHMARKS.md](../BENCHMARKS.md).

## Requisitos

- Windows 10, Windows 11 o Windows Server 2019 o posterior, x64
- PHP 8.2 o posterior para Windows, x64. El instalador puede descargarlo.

## Instalación

Descargue `OpenLiteSpeed-1.9.4-win64-setup.exe` desde [Releases](https://github.com/NeuralFoundry/openlitespeed-windows/releases/latest) y ejecútelo. El instalador muestra las versiones de PHP que encontró, sugiere el tipo de instalación y pregunta por la edición.

<img src="../setup.png" width="576" alt="Instalador de OpenLiteSpeed para Windows con la elección de edición y la versión de PHP">

| Edición | Incluye |
|---|---|
| Full stack | OpenLiteSpeed, PHP, la aplicación de la bandeja, el panel, MariaDB, phpMyAdmin y Cloudflare Tunnel. El instalador descarga unos 170 MB y verifica cada archivo con un SHA-256 fijado. |
| Solo servidor web | OpenLiteSpeed, PHP y la aplicación de la bandeja, con sitios nuevos, HTTPS de confianza, cambio de versión de PHP, importaciones y actualizaciones |

Al volver a ejecutar el instalador se cambia de edición. Al cambiar a solo servidor web se conservan las bases de datos. La edición full stack funciona con la aplicación de la bandeja, por lo que la opción de servicio de Windows es solo para la edición de solo servidor web.

El instalador limita el acceso a la carpeta de instalación a su cuenta, a los administradores y al sistema, de modo que otros usuarios del equipo no pueden modificar los programas ni la configuración. No cambia las carpetas de Laragon y XAMPP, cuyas carpetas de PHP suelen poder modificar otros usuarios; con el servicio de Windows, eso les permite ejecutar código con la cuenta del servicio, por lo que la instalación del servicio advierte sobre esas carpetas de PHP.

| Tipo de instalación | Carpeta de instalación | Carpeta de proyectos | Se inicia con |
|---|---|---|---|
| Servidor independiente | `C:\OpenLiteSpeed` | `C:\OpenLiteSpeed\www` | Aplicación de la bandeja o servicio de Windows |
| Dentro de Laragon | `C:\laragon\bin\openlitespeed` | Raíz de documentos de Laragon | Laragon **Start All** |
| Dentro de XAMPP | `C:\xampp\openlitespeed` | `C:\xampp\htdocs` | Aplicación de la bandeja o servicio de Windows |

El sitio web responde en el puerto 80, o en el 8088 si el puerto 80 está ocupado. WebAdmin está en `https://localhost:7080/` con el usuario `admin` y la contraseña que se muestra al final de la instalación. Si ejecuta el instalador sobre una instalación existente, la actualiza y conserva la configuración.

<img src="../webadmin.png" width="880" alt="Consola WebAdmin de OpenLiteSpeed ejecutándose en Windows">

Instalación desatendida:

```bat
OpenLiteSpeed-1.9.4-win64-setup.exe --silent --mode standalone --download-php 8.4 --edition full
```

| Opción | Descripción |
|---|---|
| `--mode standalone\|laragon\|xampp` | Tipo de instalación, se detecta si se omite |
| `--edition server\|full` | Edición; si se omite, `server` para una instalación nueva y la actual para una actualización. `--service` requiere `server` |
| `--php <folder>` / `--download-php 8.2\|8.3\|8.4\|8.5` | PHP que se usará |
| `--dir <folder>` / `--www <folder>` | Carpeta de instalación / carpeta de proyectos |
| `--port <n>` / `--admin-port <n>` | Puertos del sitio web y de WebAdmin |
| `--service` / `--no-tray` | Instalar el servicio de Windows / no iniciar la aplicación de la bandeja |
| `--log <file>` | Archivo de registro, `%TEMP%\OpenLiteSpeed-setup.log` por defecto, incluye la contraseña de administrador |

Los mismos archivos están disponibles en un zip con `install.ps1` para despliegues mediante scripts. Igual que el instalador, limita el acceso a la carpeta de instalación a su cuenta, a los administradores y al sistema cuando la carpeta todavía hereda los permisos de la carpeta superior:

```powershell
powershell -ExecutionPolicy Bypass -File install.ps1 -Mode standalone -DownloadPhp 8.4 -Edition full
```

Para desinstalar, use **Configuración > Aplicaciones** o ejecute `uninstall.exe --uninstall --silent` en la carpeta de instalación. La carpeta de proyectos y la carpeta `data` con las bases de datos se conservan.

## Panel

La edición full stack abre el panel desde el menú Inicio o con doble clic en el icono de la bandeja. Es una ventana de escritorio, no una página web, por lo que no escucha en ningún puerto. Las páginas:

| Página | Qué puede hacer |
|---|---|
| Overview | Iniciar y detener el servidor web, MariaDB y Cloudflare Tunnel, ver las solicitudes por segundo y las conexiones abiertas |
| Sites | Crear y eliminar sitios, elegir la versión de PHP o Node.js de cada sitio, publicar un sitio en internet, abrir su carpeta, una terminal o VS Code en ella, ejecutar una carpeta como aplicación Node.js o Python, ejecutar tareas en segundo plano |
| PHP | Instalar versiones de PHP, elegir la predeterminada, activar y desactivar extensiones y Xdebug, cambiar ajustes de `php.ini`, instalar Composer |
| Node.js | Instalar versiones de Node.js, elegir la que ejecuta las aplicaciones, reiniciar una aplicación y leer su registro |
| Databases | Crear una base de datos y su usuario, exportar e importar archivos `.sql`, eliminar una base de datos, abrir phpMyAdmin |
| Mail | Activar y desactivar el capturador de correo, leer el correo capturado con su parte HTML y sus adjuntos |
| Logs | Leer los registros del servidor, de MariaDB, de las aplicaciones y del túnel, filtrar las líneas, seguir las líneas nuevas |
| Settings | Inicio con Windows, la carpeta de proyectos, HTTPS de confianza, los puertos HTTP y de WebAdmin y la contraseña de WebAdmin |

**Terminal**, abajo a la izquierda, abre un símbolo del sistema con el PHP predeterminado, Composer, Node.js y el cliente de MariaDB en el `PATH`.

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../panel-sites-dark.png">
  <img src="../panel-sites-light.png" width="880" alt="Página Sites del panel con una versión de PHP para cada sitio, aplicaciones Node.js y Python, un sitio proxy y los interruptores Online">
</picture>

El panel necesita Microsoft Edge WebView2 Runtime, que viene con Windows 11 y con las versiones actuales de Windows 10.

## Sitios locales

Cada carpeta de la carpeta de proyectos es un sitio en `http://folder.test/`. **New site** en el panel o en la bandeja crea la carpeta. El panel ofrece estos tipos de sitio:

| Tipo | Qué obtiene |
|---|---|
| Sitio PHP | Una carpeta con un `index.php` |
| WordPress | La última versión de WordPress. Si MariaDB está instalado, una base de datos y un usuario para el sitio y un `wp-config.php` ya completado, de modo que la instalación de WordPress solo pide el título del sitio y una cuenta de administrador |
| Laravel | Un proyecto Laravel nuevo creado con `composer create-project`, servido desde su carpeta `public` |
| Aplicación Node.js | Un `server.js` y un `package.json` sencillos, iniciados por el servidor con el Node.js predeterminado |
| Aplicación Python | Un `wsgi.py` WSGI, iniciado por el servidor con el Python que encuentre en el equipo |

El instalador de WordPress solo se abre desde este equipo, así que nadie más en la red puede completarlo antes que usted. Una carpeta con un `package.json` se convierte en una aplicación Node.js con **More > Run as a Node.js app**, y **Run as a PHP site** la devuelve a su estado anterior. **Remove site** mueve la carpeta a la Papelera de reciclaje y conserva las bases de datos.

**New proxy site** da su propio nombre a una aplicación que escucha en un puerto local, por ejemplo `app.test` para un servidor de desarrollo de Vite en el puerto 5173. Las conexiones WebSocket se transmiten. Un sitio proxy solo responde a este equipo, igual que la aplicación que tiene detrás.

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../panel-site-dark.png">
  <img src="../panel-site-light.png" width="420" alt="Diálogo New site con el nombre del sitio y los tipos de sitio: PHP, WordPress, Laravel, aplicación Node.js y aplicación Python">
</picture>
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../panel-proxy-dark.png">
  <img src="../panel-proxy-light.png" width="420" alt="Diálogo New proxy site con el nombre del sitio y el puerto de la aplicación">
</picture>

## Publicar un sitio en internet

Active **Online** junto a un sitio en el panel o ejecute `openlitespeed --tunnel start <site>`. Cloudflare asigna al sitio una dirección aleatoria `https://*.trycloudflare.com` que cualquiera puede abrir hasta que usted la desactive. Funciona detrás de NAT, de un router o de una IP local porque la conexión sale desde su equipo, y no se abre nada en el firewall. Los visitantes conservan su propia dirección IP en los registros y en PHP. La dirección pública se muestra debajo de la local, con **Copy** al lado.

Para usar su propio dominio, cree un túnel en Cloudflare Zero Trust, apunte su nombre de host público a `http://127.0.0.1` en el puerto del sitio web con el nombre del sitio como encabezado HTTP Host y pegue el token del túnel en el panel, en **Use your own domain**. El token se guarda cifrado para su cuenta de Windows.

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../panel-share-dark.png">
  <img src="../panel-share-light.png" width="420" alt="Diálogo para compartir un sitio local en internet a través de Cloudflare">
</picture>
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../panel-token-dark.png">
  <img src="../panel-token-light.png" width="420" alt="Diálogo para conectar su propio dominio con un token de túnel de Cloudflare">
</picture>

Solo los sitios de la carpeta de proyectos y los sitios proxy pueden publicarse en internet. Al compartir un sitio proxy, el túnel se conecta directamente a su aplicación; con su propio dominio, apunte también ese nombre de host al puerto de la aplicación. phpMyAdmin y WebAdmin solo responden a este equipo, y un túnel no se inicia si el servidor no puede distinguir a sus visitantes de este equipo.

## Bases de datos

MariaDB escucha solo en `127.0.0.1` y se ejecuta con su cuenta, nunca como administrador. El instalador lo descarga y la aplicación de la bandeja crea las bases de datos la primera vez que se inicia. La contraseña de root se genera en ese momento y se guarda en `data\client.cnf`, que solo pueden leer su cuenta y los administradores. `SELECT ... INTO OUTFILE` y `LOAD DATA` solo funcionan en `data\files`. **Copy password** en el panel o en la bandeja la copia al portapapeles sin añadirla al historial del portapapeles. phpMyAdmin está en `http://phpmyadmin.test/`, solo para este equipo.

Un sitio de WordPress creado desde la bandeja o el panel recibe su propia base de datos y su propio usuario, y `wp-config.php` se completa automáticamente.

En la página **Databases**, **New database** crea una base de datos con un usuario del mismo nombre y una contraseña generada, que se muestra una sola vez con un botón **Copy**. **Export** escribe un archivo `.sql` con `mariadb-dump`, **Import** carga un archivo `.sql` en la base de datos y **Remove** le pide primero que escriba el nombre y también puede eliminar el usuario.

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../panel-db-dark.png">
  <img src="../panel-db-light.png" width="880" alt="Página Databases del panel con MariaDB, la contraseña de root y las opciones de exportar, importar y eliminar para cada base de datos">
</picture>

## HTTPS de confianza

**Trusted HTTPS** en el panel o en la bandeja, o `openlitespeed --dev-https on`, crea una autoridad de certificación en su equipo, pide a Windows que confíe en ella para su cuenta y sirve cada sitio local en `https://name.test/`. La autoridad está limitada mediante restricciones de nombre (name constraints) a los nombres `.test`, `.localhost` e `.internal` y a `127.0.0.1` y `::1`, por lo que no puede firmar un certificado para ningún otro sitio. Al desactivarla se elimina del almacén de confianza de Windows.

## PHP

`lsphp.exe` es la SAPI de PHP para LiteSpeed compilada para Windows, para PHP 8.2 a 8.5 en variantes thread safe y non thread safe. Funciona sobre las compilaciones oficiales de PHP de [windows.php.net](https://windows.php.net), así que las extensiones existentes y los ajustes de `php.ini` siguen funcionando. Cuando una versión de PHP no puede cargarla, OpenLiteSpeed recurre a `php-cgi.exe` mediante FastCGI.

<img src="../phpinfo.png" width="640" alt="phpinfo de PHP 8.4 en Windows con Server API LiteSpeed">

Cambie la versión de PHP desde el panel, el menú de la bandeja o con `openlitespeed --set-php <folder>`.

**Install PHP** en la página PHP muestra las versiones de windows.php.net, de la 7.4 a la 8.5, thread safe y non thread safe. Cada descarga se verifica con el SHA-256 que publica windows.php.net, se descomprime en `php\<version>` y recibe un `php.ini` con las extensiones habituales activadas. Las versiones desde la 8.2 se ejecutan mediante LSAPI y las anteriores mediante FastCGI.

Cada sitio puede usar su propia versión de PHP: elíjala en la columna PHP de la página Sites o ejecute `openlitespeed --site php <site> <folder>`. Los sitios sin una versión elegida usan la predeterminada. Para cada versión, el menú **More** tiene **Extensions**, con un interruptor para cada extensión de ese PHP, y **Settings**, para el límite de memoria, el tamaño de subida y de POST, el tiempo de ejecución, las variables de entrada, la visualización de errores y la zona horaria. **Edit php.ini** abre el archivo en el Bloc de notas. Los cambios recargan el servidor web.

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../panel-php-dark.png">
  <img src="../panel-php-light.png" width="880" alt="Página PHP del panel con tres versiones de PHP, la predeterminada, los sitios que usan cada versión y Composer">
</picture>

**Install** en la sección de Composer descarga `composer.phar` de getcomposer.org y lo verifica con su SHA-256. La terminal del panel tiene `composer` en el `PATH`, y los sitios Laravel se crean con él.

## Xdebug

El interruptor de la columna Xdebug de la página PHP, o `openlitespeed --php xdebug <folder> on`, activa Xdebug para esa versión de PHP. Si ya se carga un Xdebug desde la carpeta `ext`, se usa ese. Si no, se descarga de xdebug.org la compilación oficial para Windows de esa versión de PHP y se verifica con un SHA-256 guardado en OpenLiteSpeed: Xdebug 3.5.3 para PHP 8.2 a 8.5, thread safe y non thread safe.

Xdebug se inicia con cada solicitud y se conecta a un depurador en `127.0.0.1:9003`. Si no hay depurador, la solicitud continúa de inmediato y no se escribe nada en los registros. El JIT de opcache está desactivado mientras Xdebug está activado y vuelve cuando se desactiva Xdebug. Los ajustes se guardan al final de `php.ini` y el servidor web se recarga para aplicarlos.

Xdebug no se puede activar mientras OpenLiteSpeed esté instalado como servicio de Windows. Cualquier programa del equipo que escuche en el puerto 9003 recibe la sesión de depuración y puede ejecutar código PHP con la cuenta del servicio, así que desactive Xdebug antes de instalar el servicio.

**VS Code:** **More > Open in VS Code** en la página Sites abre la carpeta del sitio en VS Code si está instalado. Inicie una sesión de depuración de PHP que escuche en el puerto 9003, por ejemplo con la extensión PHP Debug, y abra el sitio en el navegador.

## Capturador de correo

El interruptor de la página Mail, o `openlitespeed --mail on`, captura todo el correo que envían los sitios locales, de modo que nada sale del equipo. La función `mail()` de PHP llega a OpenLiteSpeed a través de `sendmail_path` en el `php.ini` de las versiones de PHP que ejecuta OpenLiteSpeed, y las aplicaciones que envían por SMTP usan el host `127.0.0.1`, el puerto 1025, sin cifrado y cualquier nombre de usuario y contraseña. Los sitios Laravel nuevos reciben estos ajustes en `.env`. Una conexión que empieza como una solicitud web se cierra, de modo que una página web abierta en el navegador no puede enviar correo al capturador.

El correo se guarda en `data\mail`, que solo puede leer su cuenta, hasta un máximo de los 1000 mensajes más recientes y 512 MB. La página Mail muestra cada mensaje con sus partes de texto y HTML y permite guardar los adjuntos. La parte HTML se muestra sin scripts y no carga nada de internet.

## Tareas en segundo plano

**Background jobs** en el menú **More** de un sitio, o `openlitespeed --job add <site> <name> <command>`, ejecuta un comando de larga duración como `php artisan queue:work`, `php artisan schedule:work` o `npm run dev` en la carpeta del sitio, con las versiones de PHP y Node.js de ese sitio al principio del `PATH`. Las tareas se inician y se detienen con el servidor web. Una tarea que termina se vuelve a iniciar tras 1, 2, 4 y hasta 60 segundos, y después de 10 reinicios en 10 minutos permanece detenida. Detener una tarea finaliza todos los procesos que inició. La salida va a `logs\jobs\<site>.<name>.log`, que se muestra en la página Logs. Con el servicio de Windows, la cuenta del servicio ejecuta las tareas y el capturador de correo, igual que ejecuta PHP.

## Node.js

**Install Node.js** en la página Node.js muestra las versiones Current y LTS de nodejs.org. Cada descarga se verifica con `SHASUMS256.txt` y se descomprime en `node\<version>`. La página también muestra las instalaciones de Node.js encontradas en `Program Files`, Laragon, nvm, Volta y en el `PATH`. **Make default** elige la versión que ejecuta las aplicaciones Node.js, y las aplicaciones aparecen debajo con **Restart** y **Log**.

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../panel-node-dark.png">
  <img src="../panel-node-light.png" width="880" alt="Página Node.js del panel con las versiones de Node.js instaladas y las aplicaciones en ejecución">
</picture>

Una aplicación Node.js no necesita cambios: `app.listen(3000)` se redirige a la dirección que le asigna el servidor. Consulte [docs/APPS.md](../APPS.md).

Cada sitio puede usar su propio Node.js: elíjalo en la columna Version de la página Sites o ejecute `openlitespeed --site node <site> <node.exe>`. Si no hay una elección, un archivo `.nvmrc` o `.node-version` que indique una versión principal como `20` selecciona el Node.js 20 instalado más reciente; en caso contrario, el sitio usa la versión predeterminada. Las tareas en segundo plano y la terminal del sitio usan la misma versión.

## Migrar desde Apache

OpenLiteSpeed lee una configuración de Apache existente y crea un sitio por cada `<VirtualHost>`, con sus dominios, raíz de documentos, certificado SSL, redirecciones, reglas de reescritura, alias, proxies, reglas de acceso y protección con contraseña. Las reglas de los archivos `.htaccess` siguen funcionando, porque OpenLiteSpeed las aplica por sí mismo.

```bat
bin\openlitespeed.exe --import-apache --dry-run
bin\openlitespeed.exe --import-apache C:\Apache24\conf\httpd.conf
bin\openlitespeed.exe -s reload
```

Sin una ruta, la configuración de Apache de Laragon, XAMPP, WampServer o `C:\Apache24` se encuentra automáticamente. `--dry-run` muestra el informe sin cambiar nada. Importar de nuevo reemplaza la importación anterior, y `--import-apache --remove` la elimina. Las reglas que no se pueden convertir aparecen en el informe. Las reglas de acceso se convierten exactamente o de forma más estricta, nunca más permisiva; la única excepción, las reglas que bloquean clientes concretos por nombre de host o variable de entorno, se notifica como advertencia. Encontrará los detalles en [docs/APACHE.md](../APACHE.md).

## Migrar desde nginx

`--import-nginx` hace lo mismo con nginx: cada bloque `server` se convierte en un sitio con sus dominios, listeners, certificado SSL, locations, `try_files`, reescrituras, redirecciones, alias, proxies, reglas de acceso y protección con contraseña, y el PHP que está detrás de `fastcgi_pass` se ejecuta con el PHP de OpenLiteSpeed.

```bat
bin\openlitespeed.exe --import-nginx --dry-run
bin\openlitespeed.exe --import-nginx C:\nginx\conf\nginx.conf
```

Encontrará los detalles en [docs/NGINX.md](../NGINX.md).

## Varios núcleos de CPU

OpenLiteSpeed inicia un worker HTTP por cada núcleo de CPU, hasta 8 de forma predeterminada. Ajuste **Number of Workers** en **Server Configuration > General** de WebAdmin, o `httpdWorkers` en `conf\httpd_config.conf`, a cualquier valor de 1 a 32. Cada worker tiene su propio pool de PHP. Consulte [docs/ARCHITECTURE.md](../ARCHITECTURE.md) para ver cómo comparten los workers los sockets de escucha en Windows.

## Línea de comandos

Ejecute los comandos en la carpeta de instalación como `bin\openlitespeed.exe`.

| Comando | Descripción |
|---|---|
| `openlitespeed -s start\|stop\|restart\|reload\|status` | Controla el servidor, `reload` es un reinicio sin tiempo de inactividad |
| `openlitespeed -d` | Se ejecuta en primer plano |
| `openlitespeed --install-service` / `--uninstall-service` | Registra o elimina el servicio de Windows |
| `openlitespeed --set-php <folder>` | Usa otra versión de PHP |
| `openlitespeed --import-apache [httpd.conf] [--dry-run]` / `--import-apache --remove` | Importa los sitios de Apache / elimina los sitios importados |
| `openlitespeed --import-nginx [nginx.conf] [--dry-run]` / `--import-nginx --remove` | Importa los sitios de nginx / elimina los sitios importados |
| `openlitespeed --acme-install [--email <address>]` | Activa los certificados automáticos de Let's Encrypt |
| `openlitespeed --modsec-install [--detection-only]` / `--modsec-install --remove` | Activa o desactiva el firewall ModSecurity con OWASP Core Rule Set |
| `openlitespeed --dev-sites <folder> [tld]` | Crea un sitio por cada subcarpeta de una carpeta |
| `openlitespeed --new-site <name> [--wordpress\|--laravel\|--node\|--python]` | Crea un sitio en la carpeta de proyectos: un sitio PHP, WordPress con su base de datos, un proyecto Laravel, una aplicación Node.js o una aplicación Python |
| `openlitespeed --site php <site> <folder>\|default` | Usa otra versión de PHP para un sitio |
| `openlitespeed --site app <site> node\|python\|off` | Ejecuta la carpeta de un sitio como aplicación Node.js o Python, o de nuevo como sitio PHP |
| `openlitespeed --site node <site> <node.exe>\|default` | Usa otra versión de Node.js para un sitio |
| `openlitespeed --php list` | Las versiones de PHP de este equipo en JSON, con los sitios y el estado de Xdebug de cada una |
| `openlitespeed --php available` / `install <8.x> [ts]` / `remove <folder>` | Lista e instala las versiones de PHP de windows.php.net, elimina una descargada |
| `openlitespeed --php ext <folder>` / `ext-set <folder> <name> on\|off` | Lista las extensiones de una versión de PHP, activa o desactiva una |
| `openlitespeed --php ini <folder>` / `ini-set <folder> <key> <value>` | Lee o cambia un ajuste habitual de `php.ini` |
| `openlitespeed --php xdebug <folder> on\|off\|status` | Activa o desactiva Xdebug para una versión de PHP, o muestra si está activado |
| `openlitespeed --node list\|available` / `install <major>` / `default <node.exe>` / `remove <node.exe>` | Administra las versiones de Node.js para las aplicaciones |
| `openlitespeed --composer install` | Descarga Composer en `tools\composer` |
| `openlitespeed --set-port http\|admin <port>` | Cambia el puerto del sitio web o de WebAdmin |
| `openlitespeed --proxy add <name> <port>` / `remove <name>` / `list` | Añade o elimina un sitio proxy para una aplicación en un puerto local |
| `openlitespeed --dev-https on [port]\|off\|status` | HTTPS de confianza para los sitios locales |
| `openlitespeed --dev-hosts` / `--dev-hosts-clear` | Añade o elimina los nombres de los sitios en el archivo hosts |
| `openlitespeed --db install\|fetch\|start\|stop\|status` | Instala, solo descarga, inicia o detiene MariaDB y phpMyAdmin |
| `openlitespeed --db list` / `create <name> [--no-user]` / `drop <name> [--user]` | Lista, crea o elimina bases de datos |
| `openlitespeed --db export <name> <file.sql>` / `import <name> <file.sql>` | Exporta una base de datos a un archivo o importa un archivo en ella |
| `openlitespeed --tunnel start <site>` / `stop <site>` / `stop --all` / `status` | Publica un sitio en internet con Cloudflare Tunnel o lo retira |
| `openlitespeed --tunnel token` / `run` / `run-stop` / `token clear` | Guarda un token de túnel desde la entrada estándar, conecta o desconecta su propio dominio, olvida el token |
| `openlitespeed --mail on\|off\|status` | Activa o desactiva el capturador de correo, o muestra si está en ejecución |
| `openlitespeed --mail list` / `remove <id>` / `save <id> <n> <file>` / `clear` | Lista el correo capturado en JSON, elimina un mensaje, guarda en un archivo el adjunto `n` (desde 0) de un mensaje o elimina todos los mensajes |
| `openlitespeed --mail sendmail [-t] [-i] [-f <from>] [<recipient>...]` | Guarda un mensaje desde la entrada estándar, como sendmail |
| `openlitespeed --job list` / `add <site> <name> <command>` / `remove <site> <name>` | Lista, añade o elimina las tareas en segundo plano de los sitios |
| `openlitespeed --job start\|stop\|restart <site> <name>` / `autostart <site> <name> on\|off` | Controla una tarea, elige si se inicia con el servidor web |
| `openlitespeed --status` | Servidor, PHP, sitios, base de datos y túneles en JSON |
| `openlitespeed --admin-password admin\conf\htpasswd <password>` | Establece la contraseña de WebAdmin |

La aplicación de la bandeja inicia, detiene y recarga el servidor, abre los sitios, WebAdmin y los registros, crea sitios y sitios proxy, activa y desactiva el HTTPS de confianza, cambia la versión de PHP e importa sitios de Apache y nginx. En la edición full stack también abre el panel y controla MariaDB. Una vez al día comprueba en GitHub si hay una nueva versión; **Update** la descarga, comprueba la firma y la suma de comprobación y ejecuta el nuevo instalador. `check=0` en `[update]` dentro de `conf\olstray.ini` desactiva la comprobación.

## Compilar desde el código fuente

Consulte [docs/BUILD.md](../BUILD.md). La compilación usa Visual Studio 2022, CMake y Ninja, y obtiene todas las dependencias en commits fijados.

## Limitaciones

- No hay cambio de usuario por host virtual (suEXEC). PHP se ejecuta con el usuario que ejecuta el servidor o con la cuenta del servicio.
- No hay `SO_REUSEPORT`. Los workers comparten un socket de escucha por dirección.
- cgroups, namespaces, bubblewrap y CageFS son funciones de Linux y no están disponibles.
- No se admiten aplicaciones Ruby mediante el contexto App Server.

## Contribuir

Los informes de errores y las pull requests son bienvenidos, consulte [CONTRIBUTING.md](../../CONTRIBUTING.md). Informe de los problemas de seguridad de forma privada como se describe en [SECURITY.md](../../SECURITY.md).

## Licencia

GPL-3.0, consulte [LICENSE](../../LICENSE). OpenLiteSpeed está desarrollado por LiteSpeed Technologies y se distribuye con licencia GPL-3.0. `lsphp.exe` se compila a partir del código fuente de PHP y está cubierto por la [PHP License](../../win/lsphp/PHP-LICENSE.txt). Los componentes de terceros se enumeran en [THIRD-PARTY-NOTICES.txt](../../win/package/THIRD-PARTY-NOTICES.txt).

Este proyecto no está afiliado a LiteSpeed Technologies ni al PHP Group, ni cuenta con su respaldo. LiteSpeed y OpenLiteSpeed son marcas comerciales de LiteSpeed Technologies, Inc.

Del mismo autor: [OpenLiteSpeed for cPanel and WHM](https://github.com/NeuralFoundry/openlitespeed-cpanel).
