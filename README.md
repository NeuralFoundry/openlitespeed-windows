# OpenLiteSpeed for Windows

Website: [openlitespeed.dev/windows](https://openlitespeed.dev/windows/)

A native Windows build of the [OpenLiteSpeed](https://openlitespeed.org) web server 1.9.4. It runs on Windows 10, Windows 11 and Windows Server without WSL, Docker or a virtual machine, serves PHP through LSAPI or FastCGI and installs on its own or inside Laragon and XAMPP. Free and open source under GPL-3.0.

![OpenLiteSpeed WebAdmin console running on Windows](docs/dashboard.png)

## Features

- Native Win32 program built with Visual C++, with a graphical installer, a tray app and a Windows service
- Uses every CPU core: up to 32 HTTP worker processes on the same ports, restarted automatically after a crash, with zero downtime reload
- PHP 8.2, 8.3, 8.4 and 8.5 through LSAPI (`lsphp.exe` for Windows is included) or FastCGI (`php-cgi.exe`)
- Finds installed PHP versions, Laragon and XAMPP and configures itself for them
- Imports Apache sites from `httpd.conf`: virtual hosts, SSL certificates, redirects, aliases, proxies, CGI, access rules and password protection, including the rules in `.htaccess` files
- CGI programs: `.exe`, `.bat` and `.cmd` files and scripts with a `#!` line such as Perl or Python, each in its own job object with a restricted token
- Local development sites: every folder in the projects folder becomes `http://folder.test/`
- HTTP/2, HTTP/3 (QUIC), TLS 1.3, Brotli, LSCache and the WebAdmin console
- Free HTTPS certificates from Let's Encrypt, requested and renewed automatically by a built-in ACME client, see [docs/ACME.md](docs/ACME.md)
- Built-in modules: LSCache, upload progress, request body parsing and Lua scripting with LuaJIT, see [docs/MODULES.md](docs/MODULES.md)
- `.htaccess` files work as in Apache: rewrite rules, access rules (`Require`, `Order`, `Allow`, `Deny`), passwords, `<Files>` sections, `Options Indexes` and `php_flag engine off`, applied within a second when a file changes
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

Download `OpenLiteSpeed-1.9.4-win64-setup.exe` from [Releases](https://github.com/NeuralFoundry/openlitespeed-windows/releases/latest) and run it. Setup shows the PHP versions it found and suggests the setup type.

<img src="docs/setup.png" width="562" alt="OpenLiteSpeed for Windows setup with the detected PHP version">

| Setup type | Install folder | Projects folder | Started by |
|---|---|---|---|
| Standalone server | `C:\OpenLiteSpeed` | `C:\OpenLiteSpeed\www` | Tray app or Windows service |
| Inside Laragon | `C:\laragon\bin\openlitespeed` | Laragon document root | Laragon **Start All** |
| Inside XAMPP | `C:\xampp\openlitespeed` | `C:\xampp\htdocs` | Tray app or Windows service |

The website answers on port 80, or 8088 when port 80 is in use. WebAdmin is at `https://localhost:7080/` with the user `admin` and the password shown at the end of setup. Running setup again on an existing installation upgrades it and keeps the configuration.

Unattended installation:

```bat
OpenLiteSpeed-1.9.4-win64-setup.exe --silent --mode standalone --download-php 8.4 --service
```

| Option | Description |
|---|---|
| `--mode standalone\|laragon\|xampp` | Setup type, detected when omitted |
| `--php <folder>` / `--download-php 8.2\|8.3\|8.4\|8.5` | PHP to use |
| `--dir <folder>` / `--www <folder>` | Install folder / projects folder |
| `--port <n>` / `--admin-port <n>` | Website and WebAdmin ports |
| `--service` / `--no-tray` | Install the Windows service / do not start the tray app |
| `--log <file>` | Log file, `%TEMP%\OpenLiteSpeed-setup.log` by default, includes the admin password |

The same files are available as a zip with `install.ps1` for scripted deployments:

```powershell
powershell -ExecutionPolicy Bypass -File install.ps1 -Mode standalone -DownloadPhp 8.4 -Service
```

To uninstall, use **Settings > Apps** or run `uninstall.exe --uninstall --silent` in the install folder.

## PHP

`lsphp.exe` is the PHP LiteSpeed SAPI built for Windows, for PHP 8.2 to 8.5 in thread safe and non thread safe variants. It runs on top of the official PHP builds from [windows.php.net](https://windows.php.net), so existing extensions and `php.ini` settings keep working. When a PHP version cannot load it, OpenLiteSpeed falls back to `php-cgi.exe` over FastCGI.

<img src="docs/phpinfo.png" width="640" alt="phpinfo of PHP 8.4 on Windows with Server API LiteSpeed">

Switch the PHP version from the tray menu or with `openlitespeed --set-php <folder>`.

## Moving from Apache

OpenLiteSpeed reads an existing Apache configuration and creates one site for every `<VirtualHost>`, with its domains, document root, SSL certificate, redirects, rewrite rules, aliases, proxies, access rules and password protection. Rules in `.htaccess` files keep working, because OpenLiteSpeed applies them itself.

```bat
bin\openlitespeed.exe --import-apache --dry-run
bin\openlitespeed.exe --import-apache C:\Apache24\conf\httpd.conf
bin\openlitespeed.exe -s reload
```

Without a path, the Apache configuration of Laragon, XAMPP, WampServer or `C:\Apache24` is found automatically. `--dry-run` prints the report without changing anything. Importing again replaces the earlier import, and `--import-apache --remove` takes it out. Rules that cannot be converted are listed in the report. Access rules are converted exactly or made stricter, never more permissive; the only exception, rules that block particular clients by host name or environment variable, is reported as a warning. Details are in [docs/APACHE.md](docs/APACHE.md).

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
| `openlitespeed --acme-install [--email <address>]` | Turn on automatic Let's Encrypt certificates |
| `openlitespeed --dev-sites <folder> [tld]` | Create a site for every subfolder of a folder |
| `openlitespeed --dev-hosts` / `--dev-hosts-clear` | Add or remove the site names in the hosts file |
| `openlitespeed --admin-password admin\conf\htpasswd <password>` | Set the WebAdmin password |

The tray app starts, stops and reloads the server, opens the sites, WebAdmin and logs, switches the PHP version and imports Apache sites (**Sites > Import Apache sites**). Once a day it checks GitHub for a new release and shows a notice when one is out; `check=0` under `[update]` in `conf\olstray.ini` turns this off.

## Building from source

See [docs/BUILD.md](docs/BUILD.md). The build uses Visual Studio 2022, CMake and Ninja, and fetches all dependencies at pinned commits.

## Limitations

- No user switching per virtual host (suEXEC). PHP runs as the user that runs the server, or as the service account.
- No `SO_REUSEPORT`. Workers share one listening socket per address.
- cgroups, namespaces, bubblewrap and CageFS are Linux features and are not available.

## Contributing

Bug reports and pull requests are welcome, see [CONTRIBUTING.md](CONTRIBUTING.md). Report security problems privately as described in [SECURITY.md](SECURITY.md).

## License

GPL-3.0, see [LICENSE](LICENSE). OpenLiteSpeed is developed by LiteSpeed Technologies and licensed under GPL-3.0. `lsphp.exe` is built from the PHP sources and covered by the [PHP License](win/lsphp/PHP-LICENSE.txt). Third-party components are listed in [THIRD-PARTY-NOTICES.txt](win/package/THIRD-PARTY-NOTICES.txt).

This project is not affiliated with or endorsed by LiteSpeed Technologies or the PHP Group. LiteSpeed and OpenLiteSpeed are trademarks of LiteSpeed Technologies, Inc.

Also by the same author: [OpenLiteSpeed for cPanel and WHM](https://github.com/NeuralFoundry/openlitespeed-cpanel).
