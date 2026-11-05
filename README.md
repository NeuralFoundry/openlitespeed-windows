**English** | [Türkçe](docs/i18n/README.tr.md) | [简体中文](docs/i18n/README.zh-CN.md) | [Español](docs/i18n/README.es.md) | [Português](docs/i18n/README.pt-BR.md) | [Русский](docs/i18n/README.ru.md) | [日本語](docs/i18n/README.ja.md) | [Deutsch](docs/i18n/README.de.md) | [Français](docs/i18n/README.fr.md) | [한국어](docs/i18n/README.ko.md) | [Bahasa Indonesia](docs/i18n/README.id.md)

# OpenLiteSpeed for Windows

Website: [openlitespeed.dev/windows](https://openlitespeed.dev/windows/)

A native Windows build of the [OpenLiteSpeed](https://openlitespeed.org) web server 1.9.4. It runs on Windows 10, Windows 11 and Windows Server without WSL, Docker or a virtual machine, serves PHP through LSAPI or FastCGI and installs on its own or inside Laragon and XAMPP. The full stack edition adds a dashboard, MariaDB, phpMyAdmin and Cloudflare Tunnel, so a site on your computer can go online even behind a local IP. From the dashboard you install PHP and Node.js versions, give every site its own PHP version, create WordPress, Laravel, Node.js and Python sites and manage the databases. Free and open source under GPL-3.0.

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="docs/panel-dark.png">
  <img src="docs/panel-light.png" width="880" alt="OpenLiteSpeed for Windows dashboard overview with the web server, MariaDB, Cloudflare Tunnel, live request numbers and the local sites">
</picture>

## Features

- Native Win32 program built with Visual C++, with a graphical installer, a tray app and a Windows service
- Two editions in one installer: the web server only, or the full stack with a dashboard, MariaDB, phpMyAdmin and Cloudflare Tunnel
- Dashboard: one window to start and stop the services, create sites, install PHP and Node.js, manage databases, read the logs, change ports and put sites online
- PHP 7.4 to 8.5 and Node.js LTS versions installed from the dashboard, checked against their published SHA-256
- A PHP version for each site, PHP extensions turned on and off with a switch, and the common `php.ini` settings in a form
- New sites as a plain PHP site, WordPress, Laravel (with Composer), a Node.js app or a Python app
- Databases: create a database with its own user, export and import `.sql` files, remove a database
- Put a local site online with Cloudflare Tunnel: a public https address for a site on your computer, with no port forwarding and no public IP. Your own domain works with a tunnel token.
- MariaDB 11.8 and phpMyAdmin 5.2 on 127.0.0.1 only, with a generated root password. Every new WordPress site gets its own database and user.
- Uses every CPU core: up to 32 HTTP worker processes on the same ports, restarted automatically after a crash, with zero downtime reload
- PHP 8.2, 8.3, 8.4 and 8.5 through LSAPI (`lsphp.exe` for Windows is included) or FastCGI (`php-cgi.exe`)
- Finds installed PHP versions, Laragon and XAMPP and configures itself for them
- Imports Apache sites from `httpd.conf`: virtual hosts, SSL certificates, redirects, aliases, proxies, CGI, access rules and password protection, including the rules in `.htaccess` files
- CGI programs: `.exe`, `.bat` and `.cmd` files and scripts with a `#!` line such as Perl or Python, each in its own job object with a restricted token
- Node.js and Python (WSGI) applications, started and restarted by the server. Node.js apps run unchanged through [openlitespeed-node](https://github.com/NeuralFoundry/openlitespeed-node), WebSockets included, see [docs/APPS.md](docs/APPS.md)
- Local development sites: every folder in the projects folder becomes `http://folder.test/`. New sites, optionally with WordPress, from the tray or the dashboard
- Trusted HTTPS for local sites: a certificate authority made on your computer that can only sign `.test`, `.localhost` and `.internal` names and the computer's own address
- Proxy sites for apps that run on a local port, such as `api.test` for an app on `127.0.0.1:3000`
- Signed updates: the tray downloads a new release, checks its signature and installs it when you confirm
- HTTP/2, HTTP/3 (QUIC), TLS 1.3, Brotli, LSCache and the WebAdmin console
- Free HTTPS certificates from Let's Encrypt, requested and renewed automatically by a built-in ACME client, see [docs/ACME.md](docs/ACME.md)
- Built-in modules: LSCache, upload progress, request body parsing and Lua scripting with LuaJIT, see [docs/MODULES.md](docs/MODULES.md)
- `.htaccess` files work as in Apache: rewrite rules, access rules (`Require`, `Order`, `Allow`, `Deny`), passwords, `<Files>` sections, `Options Indexes` and `php_flag engine off`, applied within a second when a file changes
- Web application firewall: ModSecurity 3 with the OWASP Core Rule Set, turned on with one command, see [docs/PROTECTION.md](docs/PROTECTION.md#web-application-firewall)
- Bot protection: reCAPTCHA or hCaptcha challenges, request, bandwidth and connection limits per client IP, and the visitor's country from MaxMind GeoIP2 or IP2Location databases for PHP and rewrite rules, see [docs/PROTECTION.md](docs/PROTECTION.md)
- LiteSpeed Cache for WordPress works out of the box: install and activate the plugin, no server configuration needed. Pages are purged automatically when content changes.
- Hardened for Windows: file names are checked against case, short name, stream and device name tricks, PHP runs with a restricted token and the service runs under a virtual account

## Performance: OpenLiteSpeed for Windows vs nginx and Apache

The native Windows build from this repository against nginx and Apache on the same Windows 11 machine, with PHP 8.4 and the same 12 CPU cores for every server. Requests per second, higher is better. Every run finished without errors.

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="docs/bench-windows-overview-dark.svg">
  <img src="docs/bench-windows-overview-light.svg" width="880" alt="OpenLiteSpeed for Windows vs nginx and Apache: requests per second for a static file, a PHP application and WordPress">
</picture>

| Server | Static file | PHP application | WordPress, uncached | PHP over HTTPS | PHP at 512 connections |
|---|--:|--:|--:|--:|--:|
| OpenLiteSpeed + LSAPI | 175,203 | 9,548 | 117 | 8,687 | 9,434 |
| OpenLiteSpeed + FastCGI |  | 9,538 | 119 | 8,961 | 9,587 |
| nginx + PHP-CGI | 60,306 | 2,949 | 122 | 472 | 462 |
| Apache + mod_php | 17,889 | 7,532 | 105 | 4,473 | 3,078 |
| Apache + PHP-CGI |  | 5,743 | 118 | 4,356 | 5,396 |

Static files do not involve PHP, so they were measured once per server. Static file, PHP application and WordPress: 20 second runs with 128 keep-alive connections, 64 for WordPress. HTTPS: 10 seconds, 128 connections. The last column is the average of a continuous 60 second stress test with 512 connections:

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="docs/bench-windows-stress-app-dark.svg">
  <img src="docs/bench-windows-stress-app-light.svg" width="880" alt="OpenLiteSpeed for Windows stress test: requests per second every 10 seconds during 60 seconds with 512 connections">
</picture>

Uncached WordPress runs at about the same speed on every server because PHP and MySQL do most of the work. With a page cache the picture changes. The home page of the same site, 64 connections:

| Server and cache | Requests/s | p99 |
|---|--:|--:|
| OpenLiteSpeed + LiteSpeed Cache plugin | 21,180 | 6.1 ms |
| Apache + mod_cache_disk | 18,362 | 6.1 ms |
| nginx + fastcgi_cache | 7,745 | 10.8 ms |

Latency, memory, the full setup and the raw data are in [docs/BENCHMARKS.md](docs/BENCHMARKS.md).

## Requirements

- Windows 10, Windows 11 or Windows Server 2019 or newer, x64
- PHP 8.2 or newer for Windows, x64. The installer can download it.

## Installation

Download `OpenLiteSpeed-1.9.4-win64-setup.exe` from [Releases](https://github.com/NeuralFoundry/openlitespeed-windows/releases/latest) and run it. Setup shows the PHP versions it found, suggests the setup type and asks for the edition.

<img src="docs/setup.png" width="576" alt="OpenLiteSpeed for Windows setup with the edition choice and the PHP version">

| Edition | Includes |
|---|---|
| Full stack | OpenLiteSpeed, PHP, the tray app, the dashboard, MariaDB, phpMyAdmin and Cloudflare Tunnel. Setup downloads about 170 MB and checks every file against a pinned SHA-256. |
| Web server only | OpenLiteSpeed, PHP and the tray app with new sites, trusted HTTPS, PHP switching, imports and updates |

Running setup again switches the edition. Switching to the web server only keeps your databases. The full stack edition runs with the tray app, so the Windows service option is for the web server only edition.

Setup limits the install folder to your account, administrators and the system, so other users of the computer cannot change the programs or the configuration. It does not change the folders of Laragon and XAMPP, whose PHP folders other users can often change; with the Windows service that lets them run code as the service account, so the service install warns about such PHP folders.

| Setup type | Install folder | Projects folder | Started by |
|---|---|---|---|
| Standalone server | `C:\OpenLiteSpeed` | `C:\OpenLiteSpeed\www` | Tray app or Windows service |
| Inside Laragon | `C:\laragon\bin\openlitespeed` | Laragon document root | Laragon **Start All** |
| Inside XAMPP | `C:\xampp\openlitespeed` | `C:\xampp\htdocs` | Tray app or Windows service |

The website answers on port 80, or 8088 when port 80 is in use. WebAdmin is at `https://localhost:7080/` with the user `admin` and the password shown at the end of setup. Running setup again on an existing installation upgrades it and keeps the configuration.

<img src="docs/webadmin.png" width="880" alt="OpenLiteSpeed WebAdmin console running on Windows">

Unattended installation:

```bat
OpenLiteSpeed-1.9.4-win64-setup.exe --silent --mode standalone --download-php 8.4 --edition full
```

| Option | Description |
|---|---|
| `--mode standalone\|laragon\|xampp` | Setup type, detected when omitted |
| `--edition server\|full` | Edition, `server` for a new installation and the current one for an upgrade when omitted. `--service` needs `server` |
| `--php <folder>` / `--download-php 8.2\|8.3\|8.4\|8.5` | PHP to use |
| `--dir <folder>` / `--www <folder>` | Install folder / projects folder |
| `--port <n>` / `--admin-port <n>` | Website and WebAdmin ports |
| `--service` / `--no-tray` | Install the Windows service / do not start the tray app |
| `--log <file>` | Log file, `%TEMP%\OpenLiteSpeed-setup.log` by default, includes the admin password |

The same files are available as a zip with `install.ps1` for scripted deployments. Like setup, it limits the install folder to your account, administrators and the system when the folder still takes its permissions from the folder above:

```powershell
powershell -ExecutionPolicy Bypass -File install.ps1 -Mode standalone -DownloadPhp 8.4 -Edition full
```

To uninstall, use **Settings > Apps** or run `uninstall.exe --uninstall --silent` in the install folder. The projects folder and the `data` folder with the databases stay.

## Dashboard

The full stack edition opens the dashboard from the Start menu, or with a double click on the tray icon. It is a desktop window, not a web page, so it does not listen on a port. The pages:

| Page | What you can do |
|---|---|
| Overview | Start and stop the web server, MariaDB and Cloudflare Tunnel, see requests per second and open connections |
| Sites | Create and remove sites, pick the PHP or Node.js version of each site, put a site online, open its folder, a terminal or VS Code in it, run a folder as a Node.js or Python app, run background jobs |
| PHP | Install PHP versions, choose the default, turn extensions and Xdebug on and off, change `php.ini` settings, install Composer |
| Node.js | Install Node.js versions, choose the one that runs the apps, restart an app and read its log |
| Databases | Create a database and its user, export and import `.sql` files, remove a database, open phpMyAdmin |
| Mail | Turn the mail catcher on and off, read the caught mail with its HTML part and attachments |
| Logs | Read the server, MariaDB, app and tunnel logs, filter the lines, follow new lines |
| Settings | Start with Windows, the projects folder, trusted HTTPS, the HTTP and WebAdmin ports and the WebAdmin password |

**Terminal** at the bottom left opens a command prompt with the default PHP, Composer, Node.js and the MariaDB client on the `PATH`.

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="docs/panel-sites-dark.png">
  <img src="docs/panel-sites-light.png" width="880" alt="Sites page of the dashboard with a PHP version for each site, Node.js and Python apps, a proxy site and the Online switches">
</picture>

The dashboard needs the Microsoft Edge WebView2 Runtime, which comes with Windows 11 and current Windows 10.

## Local sites

Every folder in the projects folder is a site at `http://folder.test/`. **New site** in the dashboard or the tray creates the folder. The dashboard offers these kinds of site:

| Kind | What you get |
|---|---|
| PHP site | A folder with an `index.php` |
| WordPress | The latest WordPress. When MariaDB is installed, a database and a user for the site and a filled in `wp-config.php`, so the WordPress setup only asks for the site title and an admin account |
| Laravel | A new Laravel project from `composer create-project`, served from its `public` folder |
| Node.js app | A small `server.js` and `package.json`, started by the server with the default Node.js |
| Python app | A WSGI `wsgi.py`, started by the server with the Python found on the computer |

The WordPress installer opens from this computer only, so nobody else on the network can finish it first. A folder with a `package.json` becomes a Node.js app with **More > Run as a Node.js app**, and **Run as a PHP site** turns it back. **Remove site** moves the folder to the Recycle Bin and keeps the databases.

**New proxy site** gives an app that listens on a local port its own name, for example `app.test` for a Vite dev server on port 5173. WebSockets are passed through. A proxy site answers this computer only, like the app behind it.

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="docs/panel-site-dark.png">
  <img src="docs/panel-site-light.png" width="420" alt="New site dialog with the site name and the kinds of site: PHP, WordPress, Laravel, Node.js app and Python app">
</picture>
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="docs/panel-proxy-dark.png">
  <img src="docs/panel-proxy-light.png" width="420" alt="New proxy site dialog with the site name and the port of the app">
</picture>

## Putting a site online

Turn on **Online** next to a site in the dashboard, or run `openlitespeed --tunnel start <site>`. Cloudflare gives the site a random `https://*.trycloudflare.com` address that anyone can open until you turn it off. It works behind NAT, a router or a local IP because the connection goes out from your computer, and nothing is opened in the firewall. Visitors keep their own IP address in the logs and in PHP. The public address is shown under the local one, with **Copy** next to it.

For your own domain, create a tunnel in Cloudflare Zero Trust, point its public hostname at `http://127.0.0.1` on the website port with the site name as the HTTP host header, and paste the tunnel token in the dashboard under **Use your own domain**. The token is stored encrypted for your Windows account.

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="docs/panel-share-dark.png">
  <img src="docs/panel-share-light.png" width="420" alt="Dialog to share a local site online through Cloudflare">
</picture>
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="docs/panel-token-dark.png">
  <img src="docs/panel-token-light.png" width="420" alt="Dialog to connect your own domain with a Cloudflare tunnel token">
</picture>

Only sites from the projects folder and proxy sites can go online. Sharing a proxy site connects the tunnel straight to its app; with your own domain, point that hostname at the app's port as well. phpMyAdmin and WebAdmin answer this computer only, and a tunnel does not start unless the server can tell its visitors apart from this computer.

## Databases

MariaDB listens on `127.0.0.1` only and runs under your account, never as administrator. Setup downloads it, and the tray creates the databases the first time it starts. The root password is generated then and stored in `data\client.cnf`, readable by your account and administrators only. `SELECT ... INTO OUTFILE` and `LOAD DATA` work only in `data\files`. **Copy password** in the dashboard or the tray puts it on the clipboard without adding it to the clipboard history. phpMyAdmin is at `http://phpmyadmin.test/`, for this computer only.

A WordPress site created from the tray or the dashboard gets its own database and user, and `wp-config.php` is filled in.

On the **Databases** page, **New database** creates a database with a user of the same name and a generated password, shown once with a **Copy** button. **Export** writes a `.sql` file with `mariadb-dump`, **Import** loads a `.sql` file into the database, and **Remove** asks you to type the name first and can remove the user too.

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="docs/panel-db-dark.png">
  <img src="docs/panel-db-light.png" width="880" alt="Databases page of the dashboard with MariaDB, the root password, and export, import and remove for each database">
</picture>

## Trusted HTTPS

**Trusted HTTPS** in the dashboard or the tray, or `openlitespeed --dev-https on`, creates a certificate authority on your computer, asks Windows to trust it for your account and serves every local site on `https://name.test/`. The authority is limited by name constraints to `.test`, `.localhost` and `.internal` names and to `127.0.0.1` and `::1`, so it cannot sign a certificate for any other site. Turning it off removes it from the Windows trust store.

## PHP

`lsphp.exe` is the PHP LiteSpeed SAPI built for Windows, for PHP 8.2 to 8.5 in thread safe and non thread safe variants. It runs on top of the official PHP builds from [windows.php.net](https://windows.php.net), so existing extensions and `php.ini` settings keep working. When a PHP version cannot load it, OpenLiteSpeed falls back to `php-cgi.exe` over FastCGI.

<img src="docs/phpinfo.png" width="640" alt="phpinfo of PHP 8.4 on Windows with Server API LiteSpeed">

Switch the PHP version from the dashboard, the tray menu or with `openlitespeed --set-php <folder>`.

**Install PHP** on the PHP page lists the versions on windows.php.net, from 7.4 to 8.5, thread safe and non thread safe. A download is checked against the SHA-256 that windows.php.net publishes, unpacked into `php\<version>` and gets a `php.ini` with the usual extensions turned on. Versions from 8.2 run through LSAPI, older ones through FastCGI.

Each site can use its own PHP version: pick it in the PHP column of the Sites page, or run `openlitespeed --site php <site> <folder>`. Sites without a choice follow the default. For every version the **More** menu has **Extensions**, a switch for each extension of that PHP, and **Settings** for the memory limit, upload and POST size, execution time, input variables, error display and time zone. **Edit php.ini** opens the file in Notepad. Changes reload the web server.

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="docs/panel-php-dark.png">
  <img src="docs/panel-php-light.png" width="880" alt="PHP page of the dashboard with three PHP versions, the default, the sites that use each version and Composer">
</picture>

**Install** under Composer downloads `composer.phar` from getcomposer.org, checked against its SHA-256. The dashboard terminal has `composer` on the `PATH`, and Laravel sites are created with it.

## Xdebug

The switch in the Xdebug column of the PHP page, or `openlitespeed --php xdebug <folder> on`, turns on Xdebug for that PHP version. An Xdebug that already loads from the `ext` folder is used. Otherwise the official Windows build for that PHP version is downloaded from xdebug.org and checked against a SHA-256 kept in OpenLiteSpeed: Xdebug 3.5.3 for PHP 8.2 to 8.5, thread safe and non thread safe.

Xdebug starts with every request and connects to a debugger on `127.0.0.1:9003`. Without a debugger the request goes on at once and nothing is written to the logs. The opcache JIT is off while Xdebug is on and comes back when Xdebug is turned off. The settings are kept at the end of `php.ini`, and the web server reloads so they apply.

Xdebug cannot be turned on while OpenLiteSpeed is installed as a Windows service. Any program on the computer that listens on port 9003 gets the debug session and can run PHP code as the service account, so turn Xdebug off before you install the service.

**VS Code:** **More > Open in VS Code** on the Sites page opens the site folder in VS Code when it is installed. Start a PHP debug session that listens on port 9003, for example with the PHP Debug extension, and open the site in the browser.

## Mail catcher

The switch on the Mail page, or `openlitespeed --mail on`, catches every mail that the local sites send, so nothing leaves the computer. PHP `mail()` goes to OpenLiteSpeed through `sendmail_path` in the `php.ini` of the PHP versions that OpenLiteSpeed runs, and apps that send through SMTP use host `127.0.0.1`, port 1025, no encryption and any user name and password. New Laravel sites get these settings in `.env`. A connection that starts like a web request is closed, so a web page in the browser cannot send mail to the catcher.

The mail is kept in `data\mail`, readable by your account only, up to the newest 1000 messages and 512 MB. The Mail page shows each message with its text and HTML parts and lets you save the attachments. The HTML part runs without scripts and loads nothing from the internet.

## Background jobs

**Background jobs** in the **More** menu of a site, or `openlitespeed --job add <site> <name> <command>`, runs a long running command such as `php artisan queue:work`, `php artisan schedule:work` or `npm run dev` in the site folder, with the PHP and Node.js versions of that site first on the `PATH`. Jobs start and stop with the web server. A job that ends is started again after 1, 2, 4 and up to 60 seconds, and after 10 restarts in 10 minutes it stays stopped. Stopping a job ends every process it started. The output goes to `logs\jobs\<site>.<name>.log`, which the Logs page shows. With the Windows service, the service account runs the jobs and the mail catcher, as it runs PHP.

## Node.js

**Install Node.js** on the Node.js page lists the current and the LTS versions from nodejs.org. A download is checked against `SHASUMS256.txt` and unpacked into `node\<version>`. The page also lists Node.js found in `Program Files`, Laragon, nvm, Volta and on the `PATH`. **Make default** chooses the version that runs the Node.js apps, and the apps are listed below with **Restart** and **Log**.

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="docs/panel-node-dark.png">
  <img src="docs/panel-node-light.png" width="880" alt="Node.js page of the dashboard with the installed Node.js versions and the running apps">
</picture>

A Node.js app needs no changes: `app.listen(3000)` is moved to the address the server gives it. See [docs/APPS.md](docs/APPS.md).

Each site can use its own Node.js: pick it in the Version column of the Sites page, or run `openlitespeed --site node <site> <node.exe>`. Without a choice, a `.nvmrc` or `.node-version` file that names a major version such as `20` picks the newest installed Node.js 20, and otherwise the default runs the site. Background jobs and the terminal of the site use the same version.

## Moving from Apache

OpenLiteSpeed reads an existing Apache configuration and creates one site for every `<VirtualHost>`, with its domains, document root, SSL certificate, redirects, rewrite rules, aliases, proxies, access rules and password protection. Rules in `.htaccess` files keep working, because OpenLiteSpeed applies them itself.

```bat
bin\openlitespeed.exe --import-apache --dry-run
bin\openlitespeed.exe --import-apache C:\Apache24\conf\httpd.conf
bin\openlitespeed.exe -s reload
```

Without a path, the Apache configuration of Laragon, XAMPP, WampServer or `C:\Apache24` is found automatically. `--dry-run` prints the report without changing anything. Importing again replaces the earlier import, and `--import-apache --remove` takes it out. Rules that cannot be converted are listed in the report. Access rules are converted exactly or made stricter, never more permissive; the only exception, rules that block particular clients by host name or environment variable, is reported as a warning. Details are in [docs/APACHE.md](docs/APACHE.md).

## Moving from nginx

`--import-nginx` does the same for nginx: every `server` block becomes a site with its domains, listeners, SSL certificate, locations, `try_files`, rewrites, redirects, aliases, proxies, access rules and password protection, and PHP behind `fastcgi_pass` runs through the PHP of OpenLiteSpeed.

```bat
bin\openlitespeed.exe --import-nginx --dry-run
bin\openlitespeed.exe --import-nginx C:\nginx\conf\nginx.conf
```

Details are in [docs/NGINX.md](docs/NGINX.md).

## Multiple CPU cores

OpenLiteSpeed starts one HTTP worker per CPU core, up to 8 by default. Set **Number of Workers** under **Server Configuration > General** in WebAdmin, or `httpdWorkers` in `conf\httpd_config.conf`, to any value from 1 to 32. Each worker has its own PHP pool. See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for how workers share the listening sockets on Windows.

## Command line

Run the commands in the install folder as `bin\openlitespeed.exe`.

| Command | Description |
|---|---|
| `openlitespeed -s start\|stop\|restart\|reload\|status` | Control the server, `reload` is a zero downtime restart |
| `openlitespeed -d` | Run in the foreground |
| `openlitespeed --install-service` / `--uninstall-service` | Register or remove the Windows service |
| `openlitespeed --set-php <folder>` | Use another PHP version |
| `openlitespeed --import-apache [httpd.conf] [--dry-run]` / `--import-apache --remove` | Import the Apache sites / remove the imported sites |
| `openlitespeed --import-nginx [nginx.conf] [--dry-run]` / `--import-nginx --remove` | Import the nginx sites / remove the imported sites |
| `openlitespeed --acme-install [--email <address>]` | Turn on automatic Let's Encrypt certificates |
| `openlitespeed --modsec-install [--detection-only]` / `--modsec-install --remove` | Turn the ModSecurity firewall with the OWASP Core Rule Set on or off |
| `openlitespeed --dev-sites <folder> [tld]` | Create a site for every subfolder of a folder |
| `openlitespeed --new-site <name> [--wordpress\|--laravel\|--node\|--python]` | Create a site in the projects folder: a PHP site, WordPress with its database, a Laravel project, a Node.js app or a Python app |
| `openlitespeed --site php <site> <folder>\|default` | Use another PHP version for one site |
| `openlitespeed --site app <site> node\|python\|off` | Run a site folder as a Node.js or Python app, or as a PHP site again |
| `openlitespeed --site node <site> <node.exe>\|default` | Use another Node.js version for one site |
| `openlitespeed --php list` | The PHP versions on this computer as JSON, with the sites and the Xdebug state of each |
| `openlitespeed --php available` / `install <8.x> [ts]` / `remove <folder>` | List and install the PHP versions from windows.php.net, remove a downloaded one |
| `openlitespeed --php ext <folder>` / `ext-set <folder> <name> on\|off` | List the extensions of a PHP version, turn one on or off |
| `openlitespeed --php ini <folder>` / `ini-set <folder> <key> <value>` | Read or change a common `php.ini` setting |
| `openlitespeed --php xdebug <folder> on\|off\|status` | Turn Xdebug on or off for a PHP version, or show whether it is on |
| `openlitespeed --node list\|available` / `install <major>` / `default <node.exe>` / `remove <node.exe>` | Manage the Node.js versions for the apps |
| `openlitespeed --composer install` | Download Composer into `tools\composer` |
| `openlitespeed --set-port http\|admin <port>` | Change the website or WebAdmin port |
| `openlitespeed --proxy add <name> <port>` / `remove <name>` / `list` | Add or remove a proxy site for an app on a local port |
| `openlitespeed --dev-https on [port]\|off\|status` | Trusted HTTPS for the local sites |
| `openlitespeed --dev-hosts` / `--dev-hosts-clear` | Add or remove the site names in the hosts file |
| `openlitespeed --db install\|fetch\|start\|stop\|status` | Install, download only, start or stop MariaDB and phpMyAdmin |
| `openlitespeed --db list` / `create <name> [--no-user]` / `drop <name> [--user]` | List, create or remove databases |
| `openlitespeed --db export <name> <file.sql>` / `import <name> <file.sql>` | Export a database to a file or import a file into it |
| `openlitespeed --tunnel start <site>` / `stop <site>` / `stop --all` / `status` | Put a site online with Cloudflare Tunnel or take it offline |
| `openlitespeed --tunnel token` / `run` / `run-stop` / `token clear` | Save a tunnel token from standard input, connect or disconnect your own domain, forget the token |
| `openlitespeed --mail on\|off\|status` | Turn the mail catcher on or off, or show whether it runs |
| `openlitespeed --mail list` / `remove <id>` / `save <id> <n> <file>` / `clear` | List the caught mail as JSON, remove one message, save attachment `n` (from 0) of a message to a file, or remove all messages |
| `openlitespeed --mail sendmail [-t] [-i] [-f <from>] [<recipient>...]` | Store a message from standard input, like sendmail |
| `openlitespeed --job list` / `add <site> <name> <command>` / `remove <site> <name>` | List, add or remove the background jobs of the sites |
| `openlitespeed --job start\|stop\|restart <site> <name>` / `autostart <site> <name> on\|off` | Control a job, choose whether it starts with the web server |
| `openlitespeed --status` | Server, PHP, sites, database and tunnels as JSON |
| `openlitespeed --admin-password admin\conf\htpasswd <password>` | Set the WebAdmin password |

The tray app starts, stops and reloads the server, opens the sites, WebAdmin and logs, creates sites and proxy sites, turns trusted HTTPS on and off, switches the PHP version and imports Apache and nginx sites. In the full stack edition it also opens the dashboard and controls MariaDB. Once a day it checks GitHub for a new release; **Update** downloads it, checks the signature and the checksum and runs the new setup. `check=0` under `[update]` in `conf\olstray.ini` turns the check off.

## Building from source

See [docs/BUILD.md](docs/BUILD.md). The build uses Visual Studio 2022, CMake and Ninja, and fetches all dependencies at pinned commits.

## Limitations

- No user switching per virtual host (suEXEC). PHP runs as the user that runs the server, or as the service account.
- No `SO_REUSEPORT`. Workers share one listening socket per address.
- cgroups, namespaces, bubblewrap and CageFS are Linux features and are not available.
- Ruby applications through the App Server context are not supported.

## Contributing

Bug reports and pull requests are welcome, see [CONTRIBUTING.md](CONTRIBUTING.md). Report security problems privately as described in [SECURITY.md](SECURITY.md).

## License

GPL-3.0, see [LICENSE](LICENSE). OpenLiteSpeed is developed by LiteSpeed Technologies and licensed under GPL-3.0. `lsphp.exe` is built from the PHP sources and covered by the [PHP License](win/lsphp/PHP-LICENSE.txt). Third-party components are listed in [THIRD-PARTY-NOTICES.txt](win/package/THIRD-PARTY-NOTICES.txt).

This project is not affiliated with or endorsed by LiteSpeed Technologies or the PHP Group. LiteSpeed and OpenLiteSpeed are trademarks of LiteSpeed Technologies, Inc.

Also by the same author: [OpenLiteSpeed for cPanel and WHM](https://github.com/NeuralFoundry/openlitespeed-cpanel).
