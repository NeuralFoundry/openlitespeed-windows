[English](../../README.md) | [Türkçe](README.tr.md) | **简体中文** | [Español](README.es.md) | [Português](README.pt-BR.md) | [Русский](README.ru.md) | [日本語](README.ja.md) | [Deutsch](README.de.md) | [Français](README.fr.md) | [한국어](README.ko.md) | [Bahasa Indonesia](README.id.md)

# OpenLiteSpeed for Windows

网站：[openlitespeed.dev/windows](https://openlitespeed.dev/windows/)

OpenLiteSpeed Windows 版是 [OpenLiteSpeed](https://openlitespeed.org) 1.9.4 Web 服务器的原生 Windows 构建版本。它无需 WSL、Docker 或虚拟机，即可在 Windows 10、Windows 11 和 Windows Server 上运行，通过 LSAPI 或 FastCGI 运行 PHP，既可以独立安装，也可以安装到 Laragon 和 XAMPP 中。全栈版还包含管理面板、MariaDB、phpMyAdmin 和 Cloudflare Tunnel，即使电脑只有内网 IP，上面的网站也能上线。在管理面板中可以安装 PHP 和 Node.js 版本，为每个网站指定独立的 PHP 版本，创建 WordPress、Laravel、Node.js 和 Python 网站，并管理数据库。基于 GPL-3.0 许可证免费开源。

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../panel-dark.png">
  <img src="../panel-light.png" width="880" alt="OpenLiteSpeed Windows 版管理面板概览：Web 服务器、MariaDB、Cloudflare Tunnel、实时请求数和本地网站">
</picture>

## 功能

- 使用 Visual C++ 构建的原生 Win32 程序，带有图形安装程序、托盘程序和 Windows 服务
- 一个安装程序包含两个版本：仅 Web 服务器，或带管理面板、MariaDB、phpMyAdmin 和 Cloudflare Tunnel 的全栈版
- 管理面板：在一个窗口中启动和停止服务、创建网站、安装 PHP 和 Node.js、管理数据库、查看日志、修改端口以及让网站上线
- 从管理面板安装 PHP 7.4 至 8.5 和 Node.js LTS 版本，并按官方公布的 SHA-256 校验
- 每个网站可使用不同的 PHP 版本，PHP 扩展通过开关启用或停用，常用的 `php.ini` 设置可在表单中修改
- 新网站可以是普通 PHP 网站、WordPress、Laravel（使用 Composer）、Node.js 应用或 Python 应用
- 数据库：创建带独立用户的数据库，导出和导入 `.sql` 文件，删除数据库
- 通过 Cloudflare Tunnel 让本地网站上线：为电脑上的网站提供一个公网 https 地址，无需端口转发，也无需公网 IP。使用隧道令牌（tunnel token）可以绑定自己的域名。
- MariaDB 11.8 和 phpMyAdmin 5.2 仅监听 127.0.0.1，root 密码自动生成。每个新建的 WordPress 网站都有自己的数据库和用户。
- 充分利用所有 CPU 核心：最多 32 个 HTTP worker 进程共用相同端口，崩溃后自动重启，支持零停机重载
- 通过 LSAPI（附带 Windows 版 `lsphp.exe`）或 FastCGI（`php-cgi.exe`）运行 PHP 8.2、8.3、8.4 和 8.5
- 自动发现已安装的 PHP 版本、Laragon 和 XAMPP，并据此完成配置
- 从 `httpd.conf` 导入 Apache 网站：虚拟主机、SSL 证书、重定向、别名、代理、CGI、访问规则和密码保护，包括 `.htaccess` 文件中的规则
- CGI 程序：`.exe`、`.bat` 和 `.cmd` 文件，以及带有 `#!` 行的脚本（例如 Perl 或 Python），每个程序都在独立的作业对象中以受限令牌运行
- 由服务器启动和重启的 Node.js 和 Python（WSGI）应用。借助 [openlitespeed-node](https://github.com/NeuralFoundry/openlitespeed-node)，Node.js 应用无需修改即可运行，包括 WebSocket，参见 [docs/APPS.md](../APPS.md)
- 本地开发网站：项目文件夹中的每个文件夹都会成为 `http://folder.test/`。可从托盘或管理面板创建新网站，并可选择安装 WordPress
- 本地网站的受信任 HTTPS：在你的电脑上生成的证书颁发机构，只能为 `.test`、`.localhost` 和 `.internal` 域名以及本机地址签发证书
- 为运行在本地端口上的应用提供代理网站，例如为 `127.0.0.1:3000` 上的应用提供 `api.test`
- 签名更新：托盘程序下载新版本，校验签名，并在你确认后安装
- HTTP/2、HTTP/3（QUIC）、TLS 1.3、Brotli、LSCache 和 WebAdmin 控制台
- 来自 Let's Encrypt 的免费 HTTPS 证书，由内置 ACME 客户端自动申请和续期，参见 [docs/ACME.md](../ACME.md)
- 内置模块：LSCache、上传进度、请求体解析，以及基于 LuaJIT 的 Lua 脚本，参见 [docs/MODULES.md](../MODULES.md)
- `.htaccess` 文件的行为与 Apache 相同：重写规则、访问规则（`Require`、`Order`、`Allow`、`Deny`）、密码、`<Files>` 段、`Options Indexes` 和 `php_flag engine off`，文件修改后一秒内生效
- Web 应用防火墙：ModSecurity 3 搭配 OWASP Core Rule Set，一条命令即可启用，参见 [docs/PROTECTION.md](../PROTECTION.md#web-application-firewall)
- 防机器人：reCAPTCHA 或 hCaptcha 验证，按客户端 IP 限制请求数、带宽和连接数，并通过 MaxMind GeoIP2 或 IP2Location 数据库为 PHP 和重写规则提供访客所在国家，参见 [docs/PROTECTION.md](../PROTECTION.md)
- LiteSpeed Cache for WordPress 开箱即用：安装并启用插件即可，无需配置服务器。内容变更时自动清除页面缓存。
- 针对 Windows 加固：检查文件名中利用大小写、短文件名、数据流和设备名的攻击手法，PHP 以受限令牌运行，服务以虚拟账户运行

## 性能：OpenLiteSpeed Windows 版对比 nginx 和 Apache

在同一台 Windows 11 机器上，用本仓库的原生 Windows 构建与 nginx 和 Apache 进行对比，PHP 版本为 8.4，每个服务器都使用同样的 12 个 CPU 核心。数值为每秒请求数，越高越好。所有测试均无错误完成。

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../bench-windows-overview-dark.svg">
  <img src="../bench-windows-overview-light.svg" width="880" alt="OpenLiteSpeed Windows 版对比 nginx 和 Apache：静态文件、PHP 应用和 WordPress 的每秒请求数">
</picture>

| 服务器 | 静态文件 | PHP 应用 | WordPress，无缓存 | 通过 HTTPS 的 PHP | 512 个连接下的 PHP |
|---|--:|--:|--:|--:|--:|
| OpenLiteSpeed + LSAPI | 175,203 | 9,548 | 117 | 8,687 | 9,434 |
| OpenLiteSpeed + FastCGI |  | 9,538 | 119 | 8,961 | 9,587 |
| nginx + PHP-CGI | 60,306 | 2,949 | 122 | 472 | 462 |
| Apache + mod_php | 17,889 | 7,532 | 105 | 4,473 | 3,078 |
| Apache + PHP-CGI |  | 5,743 | 118 | 4,356 | 5,396 |

静态文件不经过 PHP，因此每个服务器只测一次。静态文件、PHP 应用和 WordPress：每轮 20 秒，128 个 keep-alive 连接，WordPress 为 64 个。HTTPS：10 秒，128 个连接。最后一列是 512 个连接下持续 60 秒压力测试的平均值：

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../bench-windows-stress-app-dark.svg">
  <img src="../bench-windows-stress-app-light.svg" width="880" alt="OpenLiteSpeed Windows 版压力测试：512 个连接、持续 60 秒期间每 10 秒的每秒请求数">
</picture>

无缓存的 WordPress 在各服务器上的速度基本相同，因为大部分工作由 PHP 和 MySQL 完成。加上页面缓存后，情况就不同了。同一网站的首页，64 个连接：

| 服务器和缓存 | 请求/秒 | p99 |
|---|--:|--:|
| OpenLiteSpeed + LiteSpeed Cache 插件 | 21,180 | 6.1 ms |
| Apache + mod_cache_disk | 18,362 | 6.1 ms |
| nginx + fastcgi_cache | 7,745 | 10.8 ms |

延迟、内存、完整测试环境和原始数据见 [docs/BENCHMARKS.md](../BENCHMARKS.md)。

## 系统要求

- Windows 10、Windows 11 或 Windows Server 2019 及更高版本，x64
- Windows 版 PHP 8.2 或更高版本，x64。安装程序可以自动下载。

## 安装

从 [Releases](https://github.com/NeuralFoundry/openlitespeed-windows/releases/latest) 下载 `OpenLiteSpeed-1.9.4-win64-setup.exe` 并运行。安装程序会列出找到的 PHP 版本，推荐安装类型，并询问要安装的版本。

<img src="../setup.png" width="576" alt="OpenLiteSpeed Windows 版安装程序：选择版本和 PHP 版本">

| 版本 | 包含内容 |
|---|---|
| 全栈版 | OpenLiteSpeed、PHP、托盘程序、管理面板、MariaDB、phpMyAdmin 和 Cloudflare Tunnel。安装程序会下载约 170 MB，并按固定的 SHA-256 校验每个文件。 |
| 仅 Web 服务器 | OpenLiteSpeed、PHP 和托盘程序，支持新建网站、受信任 HTTPS、切换 PHP、导入和更新 |

再次运行安装程序可以切换版本。切换为仅 Web 服务器版时会保留数据库。全栈版依赖托盘程序运行，因此 Windows 服务选项仅适用于仅 Web 服务器版。

安装程序会将安装目录的访问权限限制为你的账户、管理员和系统，这样电脑上的其他用户就无法修改程序或配置。它不会修改 Laragon 和 XAMPP 的文件夹，而这些软件的 PHP 文件夹通常可以被其他用户修改；在使用 Windows 服务时，这会让他们能够以服务账户身份运行代码，因此安装服务时会对这类 PHP 文件夹发出警告。

| 安装类型 | 安装目录 | 项目文件夹 | 启动方式 |
|---|---|---|---|
| 独立服务器 | `C:\OpenLiteSpeed` | `C:\OpenLiteSpeed\www` | 托盘程序或 Windows 服务 |
| 安装到 Laragon | `C:\laragon\bin\openlitespeed` | Laragon 文档根目录 | Laragon **Start All** |
| 安装到 XAMPP | `C:\xampp\openlitespeed` | `C:\xampp\htdocs` | 托盘程序或 Windows 服务 |

网站监听 80 端口，如果 80 端口已被占用则使用 8088。WebAdmin 地址为 `https://localhost:7080/`，用户名为 `admin`，密码在安装结束时显示。在已有安装上再次运行安装程序会进行升级并保留配置。

<img src="../webadmin.png" width="880" alt="在 Windows 上运行的 OpenLiteSpeed WebAdmin 控制台">

静默安装：

```bat
OpenLiteSpeed-1.9.4-win64-setup.exe --silent --mode standalone --download-php 8.4 --edition full
```

| 选项 | 说明 |
|---|---|
| `--mode standalone\|laragon\|xampp` | 安装类型，省略时自动检测 |
| `--edition server\|full` | 版本，省略时新安装为 `server`，升级时保持当前版本。`--service` 需要 `server` |
| `--php <folder>` / `--download-php 8.2\|8.3\|8.4\|8.5` | 要使用的 PHP |
| `--dir <folder>` / `--www <folder>` | 安装目录 / 项目文件夹 |
| `--port <n>` / `--admin-port <n>` | 网站端口和 WebAdmin 端口 |
| `--service` / `--no-tray` | 安装 Windows 服务 / 不启动托盘程序 |
| `--log <file>` | 日志文件，默认为 `%TEMP%\OpenLiteSpeed-setup.log`，其中包含管理员密码 |

同样的文件也提供 zip 包，附带 `install.ps1`，适合脚本化部署。与安装程序一样，如果安装目录仍从上级文件夹继承权限，它会将访问权限限制为你的账户、管理员和系统：

```powershell
powershell -ExecutionPolicy Bypass -File install.ps1 -Mode standalone -DownloadPhp 8.4 -Edition full
```

卸载时，可使用 **设置 > 应用**，或在安装目录中运行 `uninstall.exe --uninstall --silent`。项目文件夹和存放数据库的 `data` 文件夹会保留。

## 管理面板

全栈版可从开始菜单打开管理面板，也可以双击托盘图标打开。它是桌面窗口而不是网页，因此不监听任何端口。页面如下：

| 页面 | 功能 |
|---|---|
| Overview | 启动和停止 Web 服务器、MariaDB 和 Cloudflare Tunnel，查看每秒请求数和当前连接数 |
| Sites | 创建和删除网站，为每个网站选择 PHP 或 Node.js 版本，让网站上线，打开网站文件夹或在其中打开终端或 VS Code，将文件夹作为 Node.js 或 Python 应用运行，运行后台任务 |
| PHP | 安装 PHP 版本，选择默认版本，启用和停用扩展及 Xdebug，修改 `php.ini` 设置，安装 Composer |
| Node.js | 安装 Node.js 版本，选择运行应用的版本，重启应用并查看其日志 |
| Databases | 创建数据库及其用户，导出和导入 `.sql` 文件，删除数据库，打开 phpMyAdmin |
| Mail | 开启和关闭邮件捕获器，查看捕获的邮件及其 HTML 部分和附件 |
| Logs | 查看服务器、MariaDB、应用和隧道日志，筛选日志行，实时跟踪新日志 |
| Settings | 开机启动、项目文件夹、受信任 HTTPS、HTTP 和 WebAdmin 端口以及 WebAdmin 密码 |

左下角的 **Terminal** 会打开一个命令提示符，`PATH` 中包含默认的 PHP、Composer、Node.js 和 MariaDB 客户端。

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../panel-sites-dark.png">
  <img src="../panel-sites-light.png" width="880" alt="管理面板的 Sites 页面：每个网站的 PHP 版本、Node.js 和 Python 应用、一个代理网站以及 Online 开关">
</picture>

管理面板需要 Microsoft Edge WebView2 Runtime，Windows 11 和较新的 Windows 10 已自带。

## 本地网站

项目文件夹中的每个文件夹都是一个网站，地址为 `http://folder.test/`。管理面板或托盘中的 **New site** 会创建该文件夹。管理面板提供以下网站类型：

| 类型 | 你会得到 |
|---|---|
| PHP 网站 | 一个包含 `index.php` 的文件夹 |
| WordPress | 最新版 WordPress。如果已安装 MariaDB，还会为网站创建数据库和用户，并填好 `wp-config.php`，WordPress 安装时只需填写网站标题和管理员账户 |
| Laravel | 通过 `composer create-project` 创建的新 Laravel 项目，从其 `public` 文件夹提供服务 |
| Node.js 应用 | 一个简单的 `server.js` 和 `package.json`，由服务器使用默认 Node.js 启动 |
| Python 应用 | 一个 WSGI `wsgi.py`，由服务器使用电脑上找到的 Python 启动 |

WordPress 安装页面只能从本机打开，因此网络中的其他人无法抢先完成安装。包含 `package.json` 的文件夹可以通过 **More > Run as a Node.js app** 变为 Node.js 应用，**Run as a PHP site** 可以切换回来。**Remove site** 会将文件夹移到回收站，并保留数据库。

**New proxy site** 为监听本地端口的应用分配一个独立域名，例如为 5173 端口上的 Vite 开发服务器分配 `app.test`。WebSocket 会被透传。代理网站与其背后的应用一样，只响应本机。

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../panel-site-dark.png">
  <img src="../panel-site-light.png" width="420" alt="New site 对话框：网站名称和网站类型，包括 PHP、WordPress、Laravel、Node.js 应用和 Python 应用">
</picture>
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../panel-proxy-dark.png">
  <img src="../panel-proxy-light.png" width="420" alt="New proxy site 对话框：网站名称和应用端口">
</picture>

## 让网站上线

在管理面板中打开网站旁边的 **Online** 开关，或运行 `openlitespeed --tunnel start <site>`。Cloudflare 会为网站分配一个随机的 `https://*.trycloudflare.com` 地址，在你关闭之前任何人都可以访问。由于连接是从你的电脑向外发起的，所以在 NAT、路由器或内网 IP 之后也能使用，防火墙无需开放任何端口。访客的真实 IP 地址会保留在日志和 PHP 中。公网地址显示在本地地址下方，旁边有 **Copy** 按钮。

如需使用自己的域名，请在 Cloudflare Zero Trust 中创建隧道，将其公共主机名指向网站端口上的 `http://127.0.0.1`，并将网站名称设为 HTTP Host 请求头，然后把隧道令牌粘贴到管理面板的 **Use your own domain** 中。令牌会针对你的 Windows 账户加密保存。

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../panel-share-dark.png">
  <img src="../panel-share-light.png" width="420" alt="通过 Cloudflare 将本地网站分享到公网的对话框">
</picture>
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../panel-token-dark.png">
  <img src="../panel-token-light.png" width="420" alt="使用 Cloudflare 隧道令牌绑定自己域名的对话框">
</picture>

只有项目文件夹中的网站和代理网站可以上线。分享代理网站时，隧道会直接连接到其应用；使用自己的域名时，也请将该主机名指向应用的端口。phpMyAdmin 和 WebAdmin 只响应本机，并且如果服务器无法区分访客与本机，隧道就不会启动。

## 数据库

MariaDB 仅监听 `127.0.0.1`，以你的账户运行，绝不以管理员身份运行。安装程序负责下载 MariaDB，托盘程序在首次启动时创建数据库。root 密码在此时生成，保存在 `data\client.cnf` 中，只有你的账户和管理员可以读取。`SELECT ... INTO OUTFILE` 和 `LOAD DATA` 只能在 `data\files` 中使用。管理面板或托盘中的 **Copy password** 会把密码复制到剪贴板，但不会写入剪贴板历史记录。phpMyAdmin 的地址为 `http://phpmyadmin.test/`，仅限本机访问。

从托盘或管理面板创建的 WordPress 网站会获得独立的数据库和用户，并自动填好 `wp-config.php`。

在 **Databases** 页面，**New database** 会创建一个数据库，以及同名用户和自动生成的密码，密码只显示一次，并附带 **Copy** 按钮。**Export** 使用 `mariadb-dump` 导出 `.sql` 文件，**Import** 将 `.sql` 文件导入数据库，**Remove** 会先要求你输入数据库名称，也可以同时删除用户。

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../panel-db-dark.png">
  <img src="../panel-db-light.png" width="880" alt="管理面板的 Databases 页面：MariaDB、root 密码，以及每个数据库的导出、导入和删除">
</picture>

## 受信任的 HTTPS

管理面板或托盘中的 **Trusted HTTPS**，或命令 `openlitespeed --dev-https on`，会在你的电脑上创建一个证书颁发机构，请求 Windows 为你的账户信任它，并在 `https://name.test/` 上为每个本地网站提供 HTTPS。该证书颁发机构通过名称约束（name constraints）限制为 `.test`、`.localhost` 和 `.internal` 域名以及 `127.0.0.1` 和 `::1`，因此无法为其他任何网站签发证书。关闭后，它会从 Windows 信任存储中移除。

## PHP

`lsphp.exe` 是为 Windows 构建的 PHP LiteSpeed SAPI，支持 PHP 8.2 至 8.5 的线程安全版和非线程安全版。它运行在 [windows.php.net](https://windows.php.net) 的官方 PHP 构建之上，因此现有扩展和 `php.ini` 设置可以继续使用。当某个 PHP 版本无法加载它时，OpenLiteSpeed 会回退到通过 FastCGI 运行 `php-cgi.exe`。

<img src="../phpinfo.png" width="640" alt="Windows 上 PHP 8.4 的 phpinfo，Server API 为 LiteSpeed">

可以在管理面板、托盘菜单中切换 PHP 版本，也可以使用 `openlitespeed --set-php <folder>`。

PHP 页面上的 **Install PHP** 会列出 windows.php.net 上从 7.4 到 8.5 的线程安全版和非线程安全版。下载的文件会按 windows.php.net 公布的 SHA-256 校验，解压到 `php\<version>`，并生成一个启用了常用扩展的 `php.ini`。8.2 及以上版本通过 LSAPI 运行，更早的版本通过 FastCGI 运行。

每个网站都可以使用自己的 PHP 版本：在 Sites 页面的 PHP 列中选择，或运行 `openlitespeed --site php <site> <folder>`。未指定的网站使用默认版本。每个版本的 **More** 菜单中都有 **Extensions** 和 **Settings**：前者为该 PHP 的每个扩展提供一个开关，后者用于设置内存限制、上传和 POST 大小、执行时间、输入变量、错误显示和时区。**Edit php.ini** 会在记事本中打开该文件。修改后会重载 Web 服务器。

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../panel-php-dark.png">
  <img src="../panel-php-light.png" width="880" alt="管理面板的 PHP 页面：三个 PHP 版本、默认版本、使用各版本的网站以及 Composer">
</picture>

Composer 下方的 **Install** 会从 getcomposer.org 下载 `composer.phar`，并按其 SHA-256 校验。管理面板终端的 `PATH` 中包含 `composer`，Laravel 网站也是用它创建的。

## Xdebug

PHP 页面 Xdebug 列中的开关，或 `openlitespeed --php xdebug <folder> on`，会为该 PHP 版本启用 Xdebug。如果 `ext` 文件夹中已有可以加载的 Xdebug，就直接使用它。否则会从 xdebug.org 下载该 PHP 版本对应的官方 Windows 构建，并按 OpenLiteSpeed 内置的 SHA-256 校验：Xdebug 3.5.3，适用于 PHP 8.2 至 8.5 的线程安全版和非线程安全版。

Xdebug 会随每个请求启动，并连接 `127.0.0.1:9003` 上的调试器。如果没有调试器，请求会立即继续，日志中也不会写入任何内容。Xdebug 开启期间 opcache JIT 处于关闭状态，关闭 Xdebug 后恢复。相关设置保存在 `php.ini` 的末尾，Web 服务器会重载以使其生效。

当 OpenLiteSpeed 以 Windows 服务方式安装时，无法开启 Xdebug。电脑上任何监听 9003 端口的程序都会接收调试会话，并能以服务账户身份运行 PHP 代码，因此请在安装服务之前关闭 Xdebug。

**VS Code:** 如果已安装 VS Code，Sites 页面上的 **More > Open in VS Code** 会在 VS Code 中打开网站文件夹。启动一个监听 9003 端口的 PHP 调试会话（例如使用 PHP Debug 扩展），然后在浏览器中打开网站。

## 邮件捕获器

Mail 页面上的开关，或 `openlitespeed --mail on`，会捕获本地网站发送的所有邮件，因此不会有邮件离开这台电脑。PHP `mail()` 通过 OpenLiteSpeed 所运行 PHP 版本的 `php.ini` 中的 `sendmail_path` 交给 OpenLiteSpeed，通过 SMTP 发信的应用使用主机 `127.0.0.1`、端口 1025、不加密，用户名和密码任意。新建的 Laravel 网站会在 `.env` 中写入这些设置。以 Web 请求形式开头的连接会被关闭，因此浏览器中的网页无法向邮件捕获器发送邮件。

邮件保存在 `data\mail` 中，只有你的账户可以读取，最多保留最新的 1000 封，总计不超过 512 MB。Mail 页面会显示每封邮件的文本和 HTML 部分，并可以保存附件。HTML 部分在禁用脚本的情况下显示，不会从互联网加载任何内容。

## 后台任务

网站 **More** 菜单中的 **Background jobs**，或 `openlitespeed --job add <site> <name> <command>`，会在网站文件夹中运行长时间运行的命令，例如 `php artisan queue:work`、`php artisan schedule:work` 或 `npm run dev`，并将该网站的 PHP 和 Node.js 版本放在 `PATH` 的最前面。任务随 Web 服务器一起启动和停止。任务退出后，会分别在 1、2、4 秒直至最多 60 秒后重新启动，如果 10 分钟内重启了 10 次，则保持停止状态。停止任务会结束它启动的所有进程。输出写入 `logs\jobs\<site>.<name>.log`，可在 Logs 页面查看。使用 Windows 服务时，任务和邮件捕获器由服务账户运行，与 PHP 相同。

## Node.js

Node.js 页面上的 **Install Node.js** 会列出 nodejs.org 上的 Current 版和 LTS 版。下载的文件会按 `SHASUMS256.txt` 校验，并解压到 `node\<version>`。该页面还会列出在 `Program Files`、Laragon、nvm、Volta 以及 `PATH` 中找到的 Node.js。**Make default** 用于选择运行 Node.js 应用的版本，下方列出各个应用，并带有 **Restart** 和 **Log**。

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../panel-node-dark.png">
  <img src="../panel-node-light.png" width="880" alt="管理面板的 Node.js 页面：已安装的 Node.js 版本和正在运行的应用">
</picture>

Node.js 应用无需修改：`app.listen(3000)` 会被转到服务器分配的地址上。参见 [docs/APPS.md](../APPS.md)。

每个网站都可以使用自己的 Node.js：在 Sites 页面的 Version 列中选择，或运行 `openlitespeed --site node <site> <node.exe>`。未指定时，如果 `.nvmrc` 或 `.node-version` 文件写明了主版本号（例如 `20`），就会选用已安装的最新 Node.js 20，否则使用默认版本运行网站。网站的后台任务和终端也使用同一版本。

## 从 Apache 迁移

OpenLiteSpeed 会读取现有的 Apache 配置，并为每个 `<VirtualHost>` 创建一个网站，包括其域名、文档根目录、SSL 证书、重定向、重写规则、别名、代理、访问规则和密码保护。`.htaccess` 文件中的规则可以继续使用，因为 OpenLiteSpeed 会自行应用这些规则。

```bat
bin\openlitespeed.exe --import-apache --dry-run
bin\openlitespeed.exe --import-apache C:\Apache24\conf\httpd.conf
bin\openlitespeed.exe -s reload
```

不指定路径时，会自动查找 Laragon、XAMPP、WampServer 或 `C:\Apache24` 的 Apache 配置。`--dry-run` 只输出报告，不做任何修改。再次导入会替换之前的导入结果，`--import-apache --remove` 则会将其移除。无法转换的规则会在报告中列出。访问规则会被精确转换或变得更严格，绝不会更宽松；唯一的例外是按主机名或环境变量阻止特定客户端的规则，这类规则会以警告形式报告。详情见 [docs/APACHE.md](../APACHE.md)。

## 从 nginx 迁移

`--import-nginx` 对 nginx 做同样的事情：每个 `server` 块都会成为一个网站，包括其域名、监听器、SSL 证书、location、`try_files`、重写、重定向、别名、代理、访问规则和密码保护，`fastcgi_pass` 后面的 PHP 则通过 OpenLiteSpeed 的 PHP 运行。

```bat
bin\openlitespeed.exe --import-nginx --dry-run
bin\openlitespeed.exe --import-nginx C:\nginx\conf\nginx.conf
```

详情见 [docs/NGINX.md](../NGINX.md)。

## 多 CPU 核心

OpenLiteSpeed 为每个 CPU 核心启动一个 HTTP worker，默认最多 8 个。可以在 WebAdmin 的 **Server Configuration > General** 中设置 **Number of Workers**，或在 `conf\httpd_config.conf` 中设置 `httpdWorkers`，取值为 1 到 32。每个 worker 都有自己的 PHP 进程池。关于 worker 在 Windows 上如何共享监听套接字，参见 [docs/ARCHITECTURE.md](../ARCHITECTURE.md)。

## 命令行

在安装目录中以 `bin\openlitespeed.exe` 运行以下命令。

| 命令 | 说明 |
|---|---|
| `openlitespeed -s start\|stop\|restart\|reload\|status` | 控制服务器，`reload` 为零停机重启 |
| `openlitespeed -d` | 在前台运行 |
| `openlitespeed --install-service` / `--uninstall-service` | 注册或删除 Windows 服务 |
| `openlitespeed --set-php <folder>` | 使用其他 PHP 版本 |
| `openlitespeed --import-apache [httpd.conf] [--dry-run]` / `--import-apache --remove` | 导入 Apache 网站 / 删除已导入的网站 |
| `openlitespeed --import-nginx [nginx.conf] [--dry-run]` / `--import-nginx --remove` | 导入 nginx 网站 / 删除已导入的网站 |
| `openlitespeed --acme-install [--email <address>]` | 启用 Let's Encrypt 证书自动申请 |
| `openlitespeed --modsec-install [--detection-only]` / `--modsec-install --remove` | 启用或停用带 OWASP Core Rule Set 的 ModSecurity 防火墙 |
| `openlitespeed --dev-sites <folder> [tld]` | 为某个文件夹的每个子文件夹创建一个网站 |
| `openlitespeed --new-site <name> [--wordpress\|--laravel\|--node\|--python]` | 在项目文件夹中创建网站：PHP 网站、带数据库的 WordPress、Laravel 项目、Node.js 应用或 Python 应用 |
| `openlitespeed --site php <site> <folder>\|default` | 为单个网站使用其他 PHP 版本 |
| `openlitespeed --site app <site> node\|python\|off` | 将网站文件夹作为 Node.js 或 Python 应用运行，或恢复为 PHP 网站 |
| `openlitespeed --site node <site> <node.exe>\|default` | 为单个网站使用其他 Node.js 版本 |
| `openlitespeed --php list` | 以 JSON 格式列出本机的 PHP 版本，以及各版本对应的网站和 Xdebug 状态 |
| `openlitespeed --php available` / `install <8.x> [ts]` / `remove <folder>` | 列出并安装 windows.php.net 上的 PHP 版本，删除已下载的版本 |
| `openlitespeed --php ext <folder>` / `ext-set <folder> <name> on\|off` | 列出某个 PHP 版本的扩展，启用或停用某个扩展 |
| `openlitespeed --php ini <folder>` / `ini-set <folder> <key> <value>` | 读取或修改常用的 `php.ini` 设置 |
| `openlitespeed --php xdebug <folder> on\|off\|status` | 为某个 PHP 版本启用或停用 Xdebug，或查看是否已启用 |
| `openlitespeed --node list\|available` / `install <major>` / `default <node.exe>` / `remove <node.exe>` | 管理应用使用的 Node.js 版本 |
| `openlitespeed --composer install` | 将 Composer 下载到 `tools\composer` |
| `openlitespeed --set-port http\|admin <port>` | 修改网站端口或 WebAdmin 端口 |
| `openlitespeed --proxy add <name> <port>` / `remove <name>` / `list` | 为本地端口上的应用添加或删除代理网站 |
| `openlitespeed --dev-https on [port]\|off\|status` | 本地网站的受信任 HTTPS |
| `openlitespeed --dev-hosts` / `--dev-hosts-clear` | 在 hosts 文件中添加或删除网站名称 |
| `openlitespeed --db install\|fetch\|start\|stop\|status` | 安装、仅下载、启动或停止 MariaDB 和 phpMyAdmin |
| `openlitespeed --db list` / `create <name> [--no-user]` / `drop <name> [--user]` | 列出、创建或删除数据库 |
| `openlitespeed --db export <name> <file.sql>` / `import <name> <file.sql>` | 将数据库导出到文件，或将文件导入数据库 |
| `openlitespeed --tunnel start <site>` / `stop <site>` / `stop --all` / `status` | 通过 Cloudflare Tunnel 让网站上线或下线 |
| `openlitespeed --tunnel token` / `run` / `run-stop` / `token clear` | 从标准输入保存隧道令牌，连接或断开自己的域名，清除令牌 |
| `openlitespeed --mail on\|off\|status` | 开启或关闭邮件捕获器，或查看是否正在运行 |
| `openlitespeed --mail list` / `remove <id>` / `save <id> <n> <file>` / `clear` | 以 JSON 格式列出捕获的邮件，删除一封邮件，将某封邮件的第 `n` 个附件（从 0 开始）保存到文件，或删除全部邮件 |
| `openlitespeed --mail sendmail [-t] [-i] [-f <from>] [<recipient>...]` | 像 sendmail 一样从标准输入接收并保存邮件 |
| `openlitespeed --job list` / `add <site> <name> <command>` / `remove <site> <name>` | 列出、添加或删除网站的后台任务 |
| `openlitespeed --job start\|stop\|restart <site> <name>` / `autostart <site> <name> on\|off` | 控制任务，选择是否随 Web 服务器启动 |
| `openlitespeed --status` | 以 JSON 格式输出服务器、PHP、网站、数据库和隧道状态 |
| `openlitespeed --admin-password admin\conf\htpasswd <password>` | 设置 WebAdmin 密码 |

托盘程序可以启动、停止和重载服务器，打开网站、WebAdmin 和日志，创建网站和代理网站，开启和关闭受信任 HTTPS，切换 PHP 版本，以及导入 Apache 和 nginx 网站。在全栈版中，它还可以打开管理面板并控制 MariaDB。它每天检查一次 GitHub 上是否有新版本；**Update** 会下载新版本，校验签名和校验和，然后运行新的安装程序。在 `conf\olstray.ini` 的 `[update]` 下设置 `check=0` 可关闭检查。

## 从源码构建

参见 [docs/BUILD.md](../BUILD.md)。构建使用 Visual Studio 2022、CMake 和 Ninja，并按固定的提交获取所有依赖。

## 限制

- 不支持按虚拟主机切换用户（suEXEC）。PHP 以运行服务器的用户或服务账户身份运行。
- 不支持 `SO_REUSEPORT`。worker 在每个地址上共享一个监听套接字。
- cgroups、namespaces、bubblewrap 和 CageFS 是 Linux 功能，无法使用。
- 不支持通过 App Server 上下文运行 Ruby 应用。

## 参与贡献

欢迎提交 Bug 报告和 Pull Request，参见 [CONTRIBUTING.md](../../CONTRIBUTING.md)。安全问题请按照 [SECURITY.md](../../SECURITY.md) 中的说明私下报告。

## 许可证

GPL-3.0，参见 [LICENSE](../../LICENSE)。OpenLiteSpeed 由 LiteSpeed Technologies 开发，采用 GPL-3.0 许可证。`lsphp.exe` 由 PHP 源码构建，适用 [PHP License](../../win/lsphp/PHP-LICENSE.txt)。第三方组件列在 [THIRD-PARTY-NOTICES.txt](../../win/package/THIRD-PARTY-NOTICES.txt) 中。

本项目与 LiteSpeed Technologies 或 PHP Group 无关，也未获得其认可。LiteSpeed 和 OpenLiteSpeed 是 LiteSpeed Technologies, Inc. 的商标。

同一作者的其他项目：[OpenLiteSpeed for cPanel and WHM](https://github.com/NeuralFoundry/openlitespeed-cpanel)。
