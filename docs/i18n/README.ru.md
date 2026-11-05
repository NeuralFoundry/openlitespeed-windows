[English](../../README.md) | [Türkçe](README.tr.md) | [简体中文](README.zh-CN.md) | [Español](README.es.md) | [Português](README.pt-BR.md) | **Русский** | [日本語](README.ja.md) | [Deutsch](README.de.md) | [Français](README.fr.md) | [한국어](README.ko.md) | [Bahasa Indonesia](README.id.md)

# OpenLiteSpeed for Windows

Сайт: [openlitespeed.dev/windows](https://openlitespeed.dev/windows/)

OpenLiteSpeed для Windows является нативной Windows-сборкой веб-сервера [OpenLiteSpeed](https://openlitespeed.org) 1.9.4. Он работает в Windows 10, Windows 11 и Windows Server без WSL, Docker и виртуальной машины, запускает PHP через LSAPI или FastCGI и устанавливается отдельно или внутрь Laragon и XAMPP. Редакция full stack добавляет панель управления, MariaDB, phpMyAdmin и Cloudflare Tunnel, поэтому сайт на вашем компьютере можно опубликовать в интернете даже за локальным IP. В панели управления можно устанавливать версии PHP и Node.js, назначать каждому сайту свою версию PHP, создавать сайты на WordPress, Laravel, Node.js и Python и управлять базами данных. Бесплатный проект с открытым исходным кодом под лицензией GPL-3.0.

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../panel-dark.png">
  <img src="../panel-light.png" width="880" alt="Обзор панели управления OpenLiteSpeed для Windows: веб-сервер, MariaDB, Cloudflare Tunnel, число запросов в реальном времени и локальные сайты">
</picture>

## Возможности

- Нативная программа Win32, собранная в Visual C++, с графическим установщиком, приложением в системном трее и службой Windows
- Две редакции в одном установщике: только веб-сервер или full stack с панелью управления, MariaDB, phpMyAdmin и Cloudflare Tunnel
- Панель управления: одно окно, чтобы запускать и останавливать службы, создавать сайты, устанавливать PHP и Node.js, управлять базами данных, читать логи, менять порты и публиковать сайты в интернете
- PHP от 7.4 до 8.5 и LTS-версии Node.js устанавливаются из панели управления и проверяются по опубликованным SHA-256
- Своя версия PHP для каждого сайта, расширения PHP включаются и выключаются переключателем, основные настройки `php.ini` собраны в форме
- Новые сайты: обычный PHP-сайт, WordPress, Laravel (с Composer), приложение Node.js или приложение Python
- Базы данных: создание базы данных с отдельным пользователем, экспорт и импорт файлов `.sql`, удаление базы данных
- Публикация локального сайта через Cloudflare Tunnel: публичный адрес https для сайта на вашем компьютере без проброса портов и без публичного IP. Собственный домен подключается с помощью токена туннеля.
- MariaDB 11.8 и phpMyAdmin 5.2 только на 127.0.0.1, со сгенерированным паролем root. Каждый новый сайт WordPress получает свою базу данных и своего пользователя.
- Использует все ядра процессора: до 32 рабочих процессов HTTP на одних и тех же портах, автоматический перезапуск после сбоя и перезагрузка без простоя
- PHP 8.2, 8.3, 8.4 и 8.5 через LSAPI (`lsphp.exe` для Windows входит в комплект) или FastCGI (`php-cgi.exe`)
- Находит установленные версии PHP, Laragon и XAMPP и настраивается под них
- Импортирует сайты Apache из `httpd.conf`: виртуальные хосты, SSL-сертификаты, редиректы, алиасы, прокси, CGI, правила доступа и защиту паролем, включая правила из файлов `.htaccess`
- CGI-программы: файлы `.exe`, `.bat` и `.cmd` и скрипты со строкой `#!`, например на Perl или Python, каждый в собственном объекте задания (job object) с ограниченным маркером доступа
- Приложения Node.js и Python (WSGI), которые запускает и перезапускает сервер. Приложения Node.js работают без изменений благодаря [openlitespeed-node](https://github.com/NeuralFoundry/openlitespeed-node), включая WebSocket, см. [docs/APPS.md](../APPS.md)
- Локальные сайты для разработки: каждая папка в папке проектов становится сайтом `http://folder.test/`. Новые сайты, при желании с WordPress, создаются из трея или из панели управления
- Доверенный HTTPS для локальных сайтов: центр сертификации, созданный на вашем компьютере, который может подписывать только имена `.test`, `.localhost` и `.internal` и собственный адрес компьютера
- Прокси-сайты для приложений на локальном порту, например `api.test` для приложения на `127.0.0.1:3000`
- Подписанные обновления: приложение в трее скачивает новый релиз, проверяет его подпись и устанавливает после вашего подтверждения
- HTTP/2, HTTP/3 (QUIC), TLS 1.3, Brotli, LSCache и консоль WebAdmin
- Бесплатные HTTPS-сертификаты Let's Encrypt, которые встроенный ACME-клиент получает и продлевает автоматически, см. [docs/ACME.md](../ACME.md)
- Встроенные модули: LSCache, прогресс загрузки, разбор тела запроса и скрипты на Lua с LuaJIT, см. [docs/MODULES.md](../MODULES.md)
- Файлы `.htaccess` работают как в Apache: правила перезаписи, правила доступа (`Require`, `Order`, `Allow`, `Deny`), пароли, секции `<Files>`, `Options Indexes` и `php_flag engine off`, изменения применяются в течение секунды
- Межсетевой экран для веб-приложений: ModSecurity 3 с OWASP Core Rule Set, включается одной командой, см. [docs/PROTECTION.md](../PROTECTION.md#web-application-firewall)
- Защита от ботов: проверки reCAPTCHA или hCaptcha, ограничения запросов, полосы пропускания и соединений на IP клиента, а также страна посетителя из баз MaxMind GeoIP2 или IP2Location для PHP и правил перезаписи, см. [docs/PROTECTION.md](../PROTECTION.md)
- LiteSpeed Cache для WordPress работает сразу: установите и активируйте плагин, настраивать сервер не нужно. Кэш страниц очищается автоматически при изменении содержимого.
- Защита с учетом особенностей Windows: имена файлов проверяются на уловки с регистром, короткими именами, потоками данных и именами устройств, PHP работает с ограниченным маркером доступа, а служба запускается под виртуальной учетной записью

## Производительность: OpenLiteSpeed для Windows в сравнении с nginx и Apache

Нативная Windows-сборка из этого репозитория в сравнении с nginx и Apache на одной и той же машине с Windows 11, с PHP 8.4 и одними и теми же 12 ядрами процессора для каждого сервера. Запросы в секунду, больше значит лучше. Все прогоны завершились без ошибок.

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../bench-windows-overview-dark.svg">
  <img src="../bench-windows-overview-light.svg" width="880" alt="OpenLiteSpeed для Windows в сравнении с nginx и Apache: запросы в секунду для статического файла, PHP-приложения и WordPress">
</picture>

| Сервер | Статический файл | PHP-приложение | WordPress без кэша | PHP через HTTPS | PHP при 512 соединениях |
|---|--:|--:|--:|--:|--:|
| OpenLiteSpeed + LSAPI | 175 203 | 9 548 | 117 | 8 687 | 9 434 |
| OpenLiteSpeed + FastCGI |  | 9 538 | 119 | 8 961 | 9 587 |
| nginx + PHP-CGI | 60 306 | 2 949 | 122 | 472 | 462 |
| Apache + mod_php | 17 889 | 7 532 | 105 | 4 473 | 3 078 |
| Apache + PHP-CGI |  | 5 743 | 118 | 4 356 | 5 396 |

Статические файлы не используют PHP, поэтому их измеряли один раз для каждого сервера. Статический файл, PHP-приложение и WordPress: прогоны по 20 секунд со 128 keep-alive-соединениями, для WordPress 64. HTTPS: 10 секунд, 128 соединений. Последний столбец содержит среднее значение непрерывного 60-секундного нагрузочного теста с 512 соединениями:

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../bench-windows-stress-app-dark.svg">
  <img src="../bench-windows-stress-app-light.svg" width="880" alt="Нагрузочный тест OpenLiteSpeed для Windows: запросы в секунду каждые 10 секунд в течение 60 секунд при 512 соединениях">
</picture>

WordPress без кэша работает примерно с одной скоростью на всех серверах, потому что основную работу выполняют PHP и MySQL. С кэшем страниц картина меняется. Главная страница того же сайта, 64 соединения:

| Сервер и кэш | Запросов/с | p99 |
|---|--:|--:|
| OpenLiteSpeed + плагин LiteSpeed Cache | 21 180 | 6,1 мс |
| Apache + mod_cache_disk | 18 362 | 6,1 мс |
| nginx + fastcgi_cache | 7 745 | 10,8 мс |

Задержки, потребление памяти, полное описание стенда и исходные данные приведены в [docs/BENCHMARKS.md](../BENCHMARKS.md).

## Требования

- Windows 10, Windows 11 или Windows Server 2019 и новее, x64
- PHP 8.2 или новее для Windows, x64. Установщик может скачать его сам.

## Установка

Скачайте `OpenLiteSpeed-1.9.4-win64-setup.exe` со страницы [Releases](https://github.com/NeuralFoundry/openlitespeed-windows/releases/latest) и запустите его. Установщик показывает найденные версии PHP, предлагает тип установки и спрашивает, какую редакцию установить.

<img src="../setup.png" width="576" alt="Установщик OpenLiteSpeed для Windows с выбором редакции и версии PHP">

| Редакция | Состав |
|---|---|
| Full stack | OpenLiteSpeed, PHP, приложение в трее, панель управления, MariaDB, phpMyAdmin и Cloudflare Tunnel. Установщик скачивает около 170 МБ и проверяет каждый файл по закрепленному SHA-256. |
| Только веб-сервер | OpenLiteSpeed, PHP и приложение в трее с созданием сайтов, доверенным HTTPS, переключением PHP, импортом и обновлениями |

Повторный запуск установщика меняет редакцию. При переходе на редакцию «только веб-сервер» базы данных сохраняются. Редакция full stack работает вместе с приложением в трее, поэтому вариант со службой Windows доступен только для редакции «только веб-сервер».

Установщик ограничивает доступ к папке установки вашей учетной записью, администраторами и системой, поэтому другие пользователи компьютера не могут изменить программы или конфигурацию. Папки Laragon и XAMPP он не меняет, а их папки PHP другие пользователи часто могут изменять; при работе в виде службы Windows это позволяет им выполнять код под учетной записью службы, поэтому установка службы предупреждает о таких папках PHP.

| Тип установки | Папка установки | Папка проектов | Чем запускается |
|---|---|---|---|
| Отдельный сервер | `C:\OpenLiteSpeed` | `C:\OpenLiteSpeed\www` | Приложение в трее или служба Windows |
| Внутри Laragon | `C:\laragon\bin\openlitespeed` | Корневая папка документов Laragon | Laragon **Start All** |
| Внутри XAMPP | `C:\xampp\openlitespeed` | `C:\xampp\htdocs` | Приложение в трее или служба Windows |

Сайт отвечает на порту 80 или на 8088, если порт 80 занят. WebAdmin доступен по адресу `https://localhost:7080/`, пользователь `admin`, пароль показывается в конце установки. Повторный запуск установщика поверх существующей установки обновляет ее и сохраняет конфигурацию.

<img src="../webadmin.png" width="880" alt="Консоль OpenLiteSpeed WebAdmin, запущенная в Windows">

Автоматическая установка:

```bat
OpenLiteSpeed-1.9.4-win64-setup.exe --silent --mode standalone --download-php 8.4 --edition full
```

| Параметр | Описание |
|---|---|
| `--mode standalone\|laragon\|xampp` | Тип установки, если не указан, определяется автоматически |
| `--edition server\|full` | Редакция; если не указана, для новой установки `server`, а при обновлении текущая. Для `--service` нужен `server` |
| `--php <folder>` / `--download-php 8.2\|8.3\|8.4\|8.5` | Какой PHP использовать |
| `--dir <folder>` / `--www <folder>` | Папка установки / папка проектов |
| `--port <n>` / `--admin-port <n>` | Порты сайта и WebAdmin |
| `--service` / `--no-tray` | Установить службу Windows / не запускать приложение в трее |
| `--log <file>` | Файл журнала, по умолчанию `%TEMP%\OpenLiteSpeed-setup.log`, содержит пароль администратора |

Те же файлы доступны в zip-архиве с `install.ps1` для установки скриптами. Как и установщик, он ограничивает доступ к папке установки вашей учетной записью, администраторами и системой, если папка все еще наследует разрешения от родительской папки:

```powershell
powershell -ExecutionPolicy Bypass -File install.ps1 -Mode standalone -DownloadPhp 8.4 -Edition full
```

Чтобы удалить программу, откройте **Параметры > Приложения** или выполните `uninstall.exe --uninstall --silent` в папке установки. Папка проектов и папка `data` с базами данных остаются на месте.

## Панель управления

В редакции full stack панель управления открывается из меню «Пуск» или двойным щелчком по значку в трее. Это окно рабочего стола, а не веб-страница, поэтому оно не слушает никакой порт. Страницы:

| Страница | Что можно сделать |
|---|---|
| Overview | Запускать и останавливать веб-сервер, MariaDB и Cloudflare Tunnel, смотреть число запросов в секунду и открытых соединений |
| Sites | Создавать и удалять сайты, выбирать версию PHP или Node.js для каждого сайта, публиковать сайт в интернете, открывать его папку, терминал или VS Code в ней, запускать папку как приложение Node.js или Python, запускать фоновые задачи |
| PHP | Устанавливать версии PHP, выбирать версию по умолчанию, включать и выключать расширения и Xdebug, менять настройки `php.ini`, устанавливать Composer |
| Node.js | Устанавливать версии Node.js, выбирать версию для приложений, перезапускать приложение и читать его лог |
| Databases | Создавать базу данных и ее пользователя, экспортировать и импортировать файлы `.sql`, удалять базу данных, открывать phpMyAdmin |
| Mail | Включать и выключать перехватчик почты, читать перехваченные письма с HTML-частью и вложениями |
| Logs | Читать логи сервера, MariaDB, приложений и туннеля, фильтровать строки, следить за новыми строками |
| Settings | Запуск вместе с Windows, папка проектов, доверенный HTTPS, порты HTTP и WebAdmin и пароль WebAdmin |

**Terminal** в левом нижнем углу открывает командную строку, в `PATH` которой есть PHP по умолчанию, Composer, Node.js и клиент MariaDB.

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../panel-sites-dark.png">
  <img src="../panel-sites-light.png" width="880" alt="Страница Sites панели управления: версия PHP для каждого сайта, приложения Node.js и Python, прокси-сайт и переключатели Online">
</picture>

Для панели управления нужен Microsoft Edge WebView2 Runtime, который входит в Windows 11 и актуальные версии Windows 10.

## Локальные сайты

Каждая папка в папке проектов является сайтом по адресу `http://folder.test/`. Кнопка **New site** в панели управления или в трее создает папку. Панель управления предлагает такие типы сайтов:

| Тип | Что вы получаете |
|---|---|
| PHP-сайт | Папку с `index.php` |
| WordPress | Последнюю версию WordPress. Если установлена MariaDB, также базу данных и пользователя для сайта и заполненный `wp-config.php`, так что установщик WordPress спрашивает только название сайта и учетную запись администратора |
| Laravel | Новый проект Laravel, созданный через `composer create-project`, который обслуживается из папки `public` |
| Приложение Node.js | Небольшие `server.js` и `package.json`, сервер запускает их с Node.js по умолчанию |
| Приложение Python | WSGI-файл `wsgi.py`, сервер запускает его с Python, найденным на компьютере |

Установщик WordPress открывается только с этого компьютера, поэтому никто другой в сети не сможет завершить установку раньше вас. Папка с `package.json` становится приложением Node.js через **More > Run as a Node.js app**, а **Run as a PHP site** возвращает ее обратно. **Remove site** перемещает папку в корзину и сохраняет базы данных.

**New proxy site** дает приложению, которое слушает локальный порт, собственное имя, например `app.test` для dev-сервера Vite на порту 5173. Соединения WebSocket передаются дальше. Прокси-сайт, как и приложение за ним, отвечает только этому компьютеру.

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../panel-site-dark.png">
  <img src="../panel-site-light.png" width="420" alt="Диалог New site с именем сайта и типами сайтов: PHP, WordPress, Laravel, приложение Node.js и приложение Python">
</picture>
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../panel-proxy-dark.png">
  <img src="../panel-proxy-light.png" width="420" alt="Диалог New proxy site с именем сайта и портом приложения">
</picture>

## Публикация сайта в интернете

Включите **Online** рядом с сайтом в панели управления или выполните `openlitespeed --tunnel start <site>`. Cloudflare выдает сайту случайный адрес `https://*.trycloudflare.com`, который может открыть кто угодно, пока вы его не выключите. Это работает за NAT, роутером или локальным IP, потому что соединение устанавливается с вашего компьютера наружу, и в межсетевом экране ничего не открывается. В логах и в PHP видны настоящие IP-адреса посетителей. Публичный адрес показывается под локальным, рядом с кнопкой **Copy**.

Для собственного домена создайте туннель в Cloudflare Zero Trust, направьте его публичное имя хоста на `http://127.0.0.1` на порту сайта с именем сайта в качестве заголовка HTTP Host и вставьте токен туннеля в панели управления в разделе **Use your own domain**. Токен хранится в зашифрованном виде для вашей учетной записи Windows.

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../panel-share-dark.png">
  <img src="../panel-share-light.png" width="420" alt="Диалог публикации локального сайта в интернете через Cloudflare">
</picture>
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../panel-token-dark.png">
  <img src="../panel-token-light.png" width="420" alt="Диалог подключения собственного домена с токеном туннеля Cloudflare">
</picture>

Опубликовать в интернете можно только сайты из папки проектов и прокси-сайты. При публикации прокси-сайта туннель подключается напрямую к его приложению; при использовании собственного домена направьте это имя хоста тоже на порт приложения. phpMyAdmin и WebAdmin отвечают только этому компьютеру, а туннель не запускается, если сервер не может отличить своих посетителей от этого компьютера.

## Базы данных

MariaDB слушает только `127.0.0.1` и работает под вашей учетной записью, никогда от имени администратора. Установщик скачивает ее, а приложение в трее создает базы данных при первом запуске. Пароль root генерируется в этот момент и хранится в `data\client.cnf`, который могут читать только ваша учетная запись и администраторы. `SELECT ... INTO OUTFILE` и `LOAD DATA` работают только в `data\files`. **Copy password** в панели управления или в трее копирует пароль в буфер обмена, не добавляя его в журнал буфера обмена. phpMyAdmin доступен по адресу `http://phpmyadmin.test/`, только с этого компьютера.

Сайт WordPress, созданный из трея или панели управления, получает свою базу данных и своего пользователя, а `wp-config.php` заполняется автоматически.

На странице **Databases** кнопка **New database** создает базу данных с одноименным пользователем и сгенерированным паролем, который показывается один раз вместе с кнопкой **Copy**. **Export** записывает файл `.sql` с помощью `mariadb-dump`, **Import** загружает файл `.sql` в базу данных, а **Remove** сначала просит ввести имя и может удалить также пользователя.

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../panel-db-dark.png">
  <img src="../panel-db-light.png" width="880" alt="Страница Databases панели управления: MariaDB, пароль root, экспорт, импорт и удаление для каждой базы данных">
</picture>

## Доверенный HTTPS

**Trusted HTTPS** в панели управления или в трее, либо `openlitespeed --dev-https on`, создает центр сертификации на вашем компьютере, просит Windows доверять ему для вашей учетной записи и обслуживает каждый локальный сайт по адресу `https://name.test/`. Центр сертификации ограничен расширением Name Constraints именами `.test`, `.localhost` и `.internal` и адресами `127.0.0.1` и `::1`, поэтому не может подписать сертификат для любого другого сайта. При выключении он удаляется из хранилища доверенных сертификатов Windows.

## PHP

`lsphp.exe` представляет собой PHP LiteSpeed SAPI, собранный для Windows, для PHP от 8.2 до 8.5 в вариантах thread safe и non thread safe. Он работает поверх официальных сборок PHP с [windows.php.net](https://windows.php.net), поэтому существующие расширения и настройки `php.ini` продолжают работать. Если версия PHP не может его загрузить, OpenLiteSpeed переключается на `php-cgi.exe` через FastCGI.

<img src="../phpinfo.png" width="640" alt="phpinfo PHP 8.4 в Windows с Server API LiteSpeed">

Версию PHP можно переключить в панели управления, в меню трея или командой `openlitespeed --set-php <folder>`.

**Install PHP** на странице PHP показывает версии с windows.php.net от 7.4 до 8.5, thread safe и non thread safe. Загруженный архив проверяется по SHA-256, который публикует windows.php.net, распаковывается в `php\<version>` и получает `php.ini` с включенными основными расширениями. Версии начиная с 8.2 работают через LSAPI, более старые через FastCGI.

Каждый сайт может использовать свою версию PHP: выберите ее в столбце PHP на странице Sites или выполните `openlitespeed --site php <site> <folder>`. Сайты без выбранной версии используют версию по умолчанию. Для каждой версии в меню **More** есть **Extensions**, где для каждого расширения этого PHP есть переключатель, и **Settings** для лимита памяти, размера загрузки и POST, времени выполнения, входных переменных, вывода ошибок и часового пояса. **Edit php.ini** открывает файл в Блокноте. После изменений веб-сервер перезагружает конфигурацию.

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../panel-php-dark.png">
  <img src="../panel-php-light.png" width="880" alt="Страница PHP панели управления: три версии PHP, версия по умолчанию, сайты, использующие каждую версию, и Composer">
</picture>

**Install** в разделе Composer скачивает `composer.phar` с getcomposer.org и проверяет его по SHA-256. В терминале панели управления `composer` есть в `PATH`, и сайты Laravel создаются с его помощью.

## Xdebug

Переключатель в столбце Xdebug на странице PHP или `openlitespeed --php xdebug <folder> on` включает Xdebug для этой версии PHP. Если Xdebug уже загружается из папки `ext`, используется он. Иначе официальная сборка для Windows под эту версию PHP скачивается с xdebug.org и проверяется по SHA-256, хранящемуся в OpenLiteSpeed: Xdebug 3.5.3 для PHP от 8.2 до 8.5, thread safe и non thread safe.

Xdebug запускается с каждым запросом и подключается к отладчику на `127.0.0.1:9003`. Если отладчика нет, запрос сразу продолжается, и в логи ничего не записывается. Пока Xdebug включен, opcache JIT выключен и снова включается после выключения Xdebug. Настройки хранятся в конце `php.ini`, а веб-сервер перезагружает конфигурацию, чтобы они вступили в силу.

Xdebug нельзя включить, пока OpenLiteSpeed установлен как служба Windows. Любая программа на компьютере, которая слушает порт 9003, получает сеанс отладки и может выполнять PHP-код под учетной записью службы, поэтому выключите Xdebug перед установкой службы.

**VS Code:** **More > Open in VS Code** на странице Sites открывает папку сайта в VS Code, если он установлен. Запустите сеанс отладки PHP, который слушает порт 9003, например с расширением PHP Debug, и откройте сайт в браузере.

## Перехватчик почты

Переключатель на странице Mail или `openlitespeed --mail on` перехватывает все письма, которые отправляют локальные сайты, поэтому ничего не покидает компьютер. PHP `mail()` попадает в OpenLiteSpeed через `sendmail_path` в `php.ini` тех версий PHP, которые запускает OpenLiteSpeed, а приложения, отправляющие почту по SMTP, используют хост `127.0.0.1`, порт 1025, без шифрования и с любыми именем пользователя и паролем. Новые сайты Laravel получают эти настройки в `.env`. Соединение, которое начинается как веб-запрос, закрывается, поэтому веб-страница в браузере не может отправить письмо в перехватчик.

Письма хранятся в `data\mail`, доступной для чтения только вашей учетной записи, до 1000 последних сообщений и 512 МБ. Страница Mail показывает каждое сообщение с текстовой и HTML-частью и позволяет сохранить вложения. HTML-часть отображается без скриптов и ничего не загружает из интернета.

## Фоновые задачи

**Background jobs** в меню **More** сайта или `openlitespeed --job add <site> <name> <command>` запускает долго работающую команду, например `php artisan queue:work`, `php artisan schedule:work` или `npm run dev`, в папке сайта, причем версии PHP и Node.js этого сайта стоят первыми в `PATH`. Задачи запускаются и останавливаются вместе с веб-сервером. Завершившаяся задача запускается снова через 1, 2, 4 и до 60 секунд, а после 10 перезапусков за 10 минут остается остановленной. Остановка задачи завершает все процессы, которые она запустила. Вывод записывается в `logs\jobs\<site>.<name>.log`, его показывает страница Logs. При работе в виде службы Windows задачи и перехватчик почты, как и PHP, запускаются под учетной записью службы.

## Node.js

**Install Node.js** на странице Node.js показывает версии Current и LTS с nodejs.org. Загруженный архив проверяется по `SHASUMS256.txt` и распаковывается в `node\<version>`. Страница также показывает Node.js, найденный в `Program Files`, Laragon, nvm, Volta и в `PATH`. **Make default** выбирает версию, на которой работают приложения Node.js, а сами приложения перечислены ниже с кнопками **Restart** и **Log**.

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../panel-node-dark.png">
  <img src="../panel-node-light.png" width="880" alt="Страница Node.js панели управления: установленные версии Node.js и работающие приложения">
</picture>

Приложение Node.js не нужно менять: `app.listen(3000)` перенаправляется на адрес, который выдает сервер. См. [docs/APPS.md](../APPS.md).

Каждый сайт может использовать свой Node.js: выберите его в столбце Version на странице Sites или выполните `openlitespeed --site node <site> <node.exe>`. Если выбора нет, файл `.nvmrc` или `.node-version` с основной версией, например `20`, выбирает самый новый установленный Node.js 20, иначе сайт работает на версии по умолчанию. Фоновые задачи и терминал сайта используют ту же версию.

## Переход с Apache

OpenLiteSpeed читает существующую конфигурацию Apache и создает по сайту для каждого `<VirtualHost>` с его доменами, корневой папкой документов, SSL-сертификатом, редиректами, правилами перезаписи, алиасами, прокси, правилами доступа и защитой паролем. Правила в файлах `.htaccess` продолжают работать, потому что OpenLiteSpeed применяет их сам.

```bat
bin\openlitespeed.exe --import-apache --dry-run
bin\openlitespeed.exe --import-apache C:\Apache24\conf\httpd.conf
bin\openlitespeed.exe -s reload
```

Без пути конфигурация Apache из Laragon, XAMPP, WampServer или `C:\Apache24` находится автоматически. `--dry-run` выводит отчет, ничего не меняя. Повторный импорт заменяет предыдущий, а `--import-apache --remove` удаляет его. Правила, которые не удалось преобразовать, перечислены в отчете. Правила доступа преобразуются точно или становятся строже, но никогда не мягче; единственное исключение, правила, блокирующие отдельных клиентов по имени хоста или переменной окружения, выводится как предупреждение. Подробности в [docs/APACHE.md](../APACHE.md).

## Переход с nginx

`--import-nginx` делает то же самое для nginx: каждый блок `server` становится сайтом с его доменами, слушателями, SSL-сертификатом, location, `try_files`, перезаписями, редиректами, алиасами, прокси, правилами доступа и защитой паролем, а PHP за `fastcgi_pass` работает через PHP в OpenLiteSpeed.

```bat
bin\openlitespeed.exe --import-nginx --dry-run
bin\openlitespeed.exe --import-nginx C:\nginx\conf\nginx.conf
```

Подробности в [docs/NGINX.md](../NGINX.md).

## Несколько ядер процессора

OpenLiteSpeed запускает по одному рабочему процессу HTTP на ядро процессора, по умолчанию не больше 8. Задайте **Number of Workers** в разделе **Server Configuration > General** в WebAdmin или `httpdWorkers` в `conf\httpd_config.conf`, любое значение от 1 до 32. У каждого рабочего процесса свой пул PHP. О том, как рабочие процессы делят слушающие сокеты в Windows, см. [docs/ARCHITECTURE.md](../ARCHITECTURE.md).

## Командная строка

Выполняйте команды в папке установки как `bin\openlitespeed.exe`.

| Команда | Описание |
|---|---|
| `openlitespeed -s start\|stop\|restart\|reload\|status` | Управление сервером, `reload` перезапускает его без простоя |
| `openlitespeed -d` | Запуск на переднем плане |
| `openlitespeed --install-service` / `--uninstall-service` | Регистрация или удаление службы Windows |
| `openlitespeed --set-php <folder>` | Использовать другую версию PHP |
| `openlitespeed --import-apache [httpd.conf] [--dry-run]` / `--import-apache --remove` | Импорт сайтов Apache / удаление импортированных сайтов |
| `openlitespeed --import-nginx [nginx.conf] [--dry-run]` / `--import-nginx --remove` | Импорт сайтов nginx / удаление импортированных сайтов |
| `openlitespeed --acme-install [--email <address>]` | Включить автоматические сертификаты Let's Encrypt |
| `openlitespeed --modsec-install [--detection-only]` / `--modsec-install --remove` | Включить или выключить межсетевой экран ModSecurity с OWASP Core Rule Set |
| `openlitespeed --dev-sites <folder> [tld]` | Создать сайт для каждой подпапки папки |
| `openlitespeed --new-site <name> [--wordpress\|--laravel\|--node\|--python]` | Создать сайт в папке проектов: PHP-сайт, WordPress с базой данных, проект Laravel, приложение Node.js или приложение Python |
| `openlitespeed --site php <site> <folder>\|default` | Использовать другую версию PHP для одного сайта |
| `openlitespeed --site app <site> node\|python\|off` | Запускать папку сайта как приложение Node.js или Python либо снова как PHP-сайт |
| `openlitespeed --site node <site> <node.exe>\|default` | Использовать другую версию Node.js для одного сайта |
| `openlitespeed --php list` | Версии PHP на этом компьютере в формате JSON с сайтами и состоянием Xdebug для каждой |
| `openlitespeed --php available` / `install <8.x> [ts]` / `remove <folder>` | Показать и установить версии PHP с windows.php.net, удалить скачанную версию |
| `openlitespeed --php ext <folder>` / `ext-set <folder> <name> on\|off` | Показать расширения версии PHP, включить или выключить расширение |
| `openlitespeed --php ini <folder>` / `ini-set <folder> <key> <value>` | Прочитать или изменить основную настройку `php.ini` |
| `openlitespeed --php xdebug <folder> on\|off\|status` | Включить или выключить Xdebug для версии PHP или показать, включен ли он |
| `openlitespeed --node list\|available` / `install <major>` / `default <node.exe>` / `remove <node.exe>` | Управление версиями Node.js для приложений |
| `openlitespeed --composer install` | Скачать Composer в `tools\composer` |
| `openlitespeed --set-port http\|admin <port>` | Изменить порт сайта или WebAdmin |
| `openlitespeed --proxy add <name> <port>` / `remove <name>` / `list` | Добавить или удалить прокси-сайт для приложения на локальном порту |
| `openlitespeed --dev-https on [port]\|off\|status` | Доверенный HTTPS для локальных сайтов |
| `openlitespeed --dev-hosts` / `--dev-hosts-clear` | Добавить или удалить имена сайтов в файле hosts |
| `openlitespeed --db install\|fetch\|start\|stop\|status` | Установить, только скачать, запустить или остановить MariaDB и phpMyAdmin |
| `openlitespeed --db list` / `create <name> [--no-user]` / `drop <name> [--user]` | Показать, создать или удалить базы данных |
| `openlitespeed --db export <name> <file.sql>` / `import <name> <file.sql>` | Экспортировать базу данных в файл или импортировать в нее файл |
| `openlitespeed --tunnel start <site>` / `stop <site>` / `stop --all` / `status` | Опубликовать сайт в интернете через Cloudflare Tunnel или снять с публикации |
| `openlitespeed --tunnel token` / `run` / `run-stop` / `token clear` | Сохранить токен туннеля из стандартного ввода, подключить или отключить собственный домен, забыть токен |
| `openlitespeed --mail on\|off\|status` | Включить или выключить перехватчик почты или показать, работает ли он |
| `openlitespeed --mail list` / `remove <id>` / `save <id> <n> <file>` / `clear` | Показать перехваченные письма в формате JSON, удалить одно письмо, сохранить вложение `n` (с 0) письма в файл или удалить все письма |
| `openlitespeed --mail sendmail [-t] [-i] [-f <from>] [<recipient>...]` | Сохранить письмо из стандартного ввода, как sendmail |
| `openlitespeed --job list` / `add <site> <name> <command>` / `remove <site> <name>` | Показать, добавить или удалить фоновые задачи сайтов |
| `openlitespeed --job start\|stop\|restart <site> <name>` / `autostart <site> <name> on\|off` | Управлять задачей, выбрать, запускается ли она вместе с веб-сервером |
| `openlitespeed --status` | Сервер, PHP, сайты, база данных и туннели в формате JSON |
| `openlitespeed --admin-password admin\conf\htpasswd <password>` | Задать пароль WebAdmin |

Приложение в трее запускает, останавливает и перезагружает сервер, открывает сайты, WebAdmin и логи, создает сайты и прокси-сайты, включает и выключает доверенный HTTPS, переключает версию PHP и импортирует сайты Apache и nginx. В редакции full stack оно также открывает панель управления и управляет MariaDB. Раз в день оно проверяет на GitHub, вышел ли новый релиз; **Update** скачивает его, проверяет подпись и контрольную сумму и запускает новый установщик. `check=0` в секции `[update]` файла `conf\olstray.ini` отключает проверку.

## Сборка из исходного кода

См. [docs/BUILD.md](../BUILD.md). Для сборки используются Visual Studio 2022, CMake и Ninja, а все зависимости загружаются с зафиксированных коммитов.

## Ограничения

- Нет переключения пользователя для каждого виртуального хоста (suEXEC). PHP работает от имени пользователя, запустившего сервер, или под учетной записью службы.
- Нет `SO_REUSEPORT`. Рабочие процессы используют один общий слушающий сокет на каждый адрес.
- cgroups, namespaces, bubblewrap и CageFS являются функциями Linux и недоступны.
- Приложения Ruby через контекст App Server не поддерживаются.

## Участие в разработке

Сообщения об ошибках и pull request приветствуются, см. [CONTRIBUTING.md](../../CONTRIBUTING.md). О проблемах безопасности сообщайте конфиденциально, как описано в [SECURITY.md](../../SECURITY.md).

## Лицензия

GPL-3.0, см. [LICENSE](../../LICENSE). OpenLiteSpeed разрабатывается компанией LiteSpeed Technologies и распространяется под лицензией GPL-3.0. `lsphp.exe` собран из исходного кода PHP и распространяется под [PHP License](../../win/lsphp/PHP-LICENSE.txt). Сторонние компоненты перечислены в [THIRD-PARTY-NOTICES.txt](../../win/package/THIRD-PARTY-NOTICES.txt).

Этот проект не связан с LiteSpeed Technologies или PHP Group и не одобрен ими. LiteSpeed и OpenLiteSpeed являются товарными знаками LiteSpeed Technologies, Inc.

Другой проект того же автора: [OpenLiteSpeed for cPanel and WHM](https://github.com/NeuralFoundry/openlitespeed-cpanel).
