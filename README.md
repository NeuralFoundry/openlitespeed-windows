# OpenLiteSpeed for Windows

Website: [openlitespeed.dev/windows](https://openlitespeed.dev/windows/)

A native Windows build of the [OpenLiteSpeed](https://openlitespeed.org) web server 1.9.4. It runs on Windows 10, Windows 11 and Windows Server without WSL, Docker or a virtual machine, serves PHP through LSAPI or FastCGI and installs on its own or inside Laragon and XAMPP. The full stack edition adds a dashboard, MariaDB, phpMyAdmin and Cloudflare Tunnel, so a site on your computer can go online even behind a local IP. Free and open source under GPL-3.0.

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="docs/panel-dark.png">
  <img src="docs/panel-light.png" width="880" alt="OpenLiteSpeed for Windows dashboard with the web server, MariaDB, Cloudflare Tunnel, PHP and the local sites">
</picture>

## Features

- Native Win32 program built with Visual C++, with a graphical installer, a tray app and a Windows service
- Two editions in one installer: the web server only, or the full stack with a dashboard, MariaDB, phpMyAdmin and Cloudflare Tunnel
- Dashboard: one window to start and stop the services, create sites, switch PHP, turn on trusted HTTPS and put sites online
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

Setup limits the install folder to your account, administrators and the system, so other users of the computer cannot change the programs or the configuration.

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

The same files are available as a zip with `install.ps1` for scripted deployments:

```powershell
powershell -ExecutionPolicy Bypass -File install.ps1 -Mode standalone -DownloadPhp 8.4 -Edition full
```

To uninstall, use **Settings > Apps** or run `uninstall.exe --uninstall --silent` in the install folder. The projects folder and the `data` folder with the databases stay.

## Dashboard

The full stack edition opens the dashboard from the Start menu, or with a double click on the tray icon. It is a desktop window, not a web page, so it does not listen on a port. It shows:

- the web server, MariaDB and Cloudflare Tunnel with their ports, each with a switch to start or stop it
- every site with its local address, and a switch to put it online
- the PHP version and trusted HTTPS, both changed with one click
- **New site**, **New proxy site**, the projects folder, WebAdmin and the error log

The dashboard needs the Microsoft Edge WebView2 Runtime, which comes with Windows 11 and current Windows 10.

## Local sites

Every folder in the projects folder is a site at `http://folder.test/`. **New site** in the dashboard or the tray creates the folder. With **Install WordPress** it also downloads the latest WordPress. When MariaDB is installed it creates a database and a user for the site and fills in `wp-config.php`, so the WordPress setup only asks for the site title and an admin account. The WordPress installer opens from this computer only, so nobody else on the network can finish it first.

**New proxy site** gives an app that listens on a local port its own name, for example `app.test` for a Node.js or Vite dev server on port 5173. WebSockets are passed through. A proxy site answers this computer only, like the app behind it.

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="docs/panel-site-dark.png">
  <img src="docs/panel-site-light.png" width="420" alt="New site dialog with the site name and the Install WordPress option">
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

Only sites from the projects folder and proxy sites can go online. Sharing a proxy site connects the tunnel straight to its app. phpMyAdmin and WebAdmin answer this computer only, and a tunnel does not start unless the server can tell its visitors apart from this computer.

## Databases

MariaDB listens on `127.0.0.1` only and runs under your account, never as administrator. Setup downloads it, and the tray creates the databases the first time it starts. The root password is generated then and stored in `data\client.cnf`, readable by your account and administrators only. `SELECT ... INTO OUTFILE` and `LOAD DATA` work only in `data\files`. **Copy password** in the dashboard or the tray puts it on the clipboard without adding it to the clipboard history. phpMyAdmin is at `http://phpmyadmin.test/`, for this computer only.

A WordPress site created from the tray or the dashboard gets its own database and user, and `wp-config.php` is filled in.

## Trusted HTTPS

**Trusted HTTPS** in the dashboard or the tray, or `openlitespeed --dev-https on`, creates a certificate authority on your computer, asks Windows to trust it for your account and serves every local site on `https://name.test/`. The authority is limited by name constraints to `.test`, `.localhost` and `.internal` names and to `127.0.0.1` and `::1`, so it cannot sign a certificate for any other site. Turning it off removes it from the Windows trust store.

## PHP

`lsphp.exe` is the PHP LiteSpeed SAPI built for Windows, for PHP 8.2 to 8.5 in thread safe and non thread safe variants. It runs on top of the official PHP builds from [windows.php.net](https://windows.php.net), so existing extensions and `php.ini` settings keep working. When a PHP version cannot load it, OpenLiteSpeed falls back to `php-cgi.exe` over FastCGI.

<img src="docs/phpinfo.png" width="640" alt="phpinfo of PHP 8.4 on Windows with Server API LiteSpeed">

Switch the PHP version from the dashboard, the tray menu or with `openlitespeed --set-php <folder>`.

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
| `openlitespeed --new-site <name> [--wordpress]` | Create a site in the projects folder, with WordPress and its database if asked |
| `openlitespeed --proxy add <name> <port>` / `remove <name>` / `list` | Add or remove a proxy site for an app on a local port |
| `openlitespeed --dev-https on [port]\|off\|status` | Trusted HTTPS for the local sites |
| `openlitespeed --dev-hosts` / `--dev-hosts-clear` | Add or remove the site names in the hosts file |
| `openlitespeed --db install\|fetch\|start\|stop\|status` | Install, download only, start or stop MariaDB and phpMyAdmin |
| `openlitespeed --tunnel start <site>` / `stop <site>` / `stop --all` / `status` | Put a site online with Cloudflare Tunnel or take it offline |
| `openlitespeed --tunnel token` / `run` / `run-stop` / `token clear` | Save a tunnel token from standard input, connect or disconnect your own domain, forget the token |
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
