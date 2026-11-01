# OpenLiteSpeed for Windows benchmarks

OpenLiteSpeed for Windows, the native Windows build of OpenLiteSpeed from this repository, compared with nginx, Apache and the PHP built-in server on one Windows machine. Each server runs with the PHP integration it normally uses on Windows, so the results compare complete deployments, not only the server engines. Apache is tested twice: with `mod_php`, which needs the thread safe PHP build, and with PHP-CGI, the same non thread safe PHP that nginx and OpenLiteSpeed use.

Every run is reported with its error count. No run was left out.

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="bench-windows-overview-dark.svg">
  <img src="bench-windows-overview-light.svg" width="880" alt="OpenLiteSpeed for Windows vs nginx and Apache: requests per second for a static file, a PHP application and WordPress">
</picture>

## Sustained load on Windows

20 second runs with PHP 8.4, 128 keep-alive connections, 64 for WordPress. p50 and p99 are per-request latencies.

### Static file, 8 KB HTML

| Server | Requests/s | p50 | p99 | Errors | Memory |
|---|--:|--:|--:|--:|--:|
| OpenLiteSpeed | 175,203 | 1.0 ms | 2.0 ms | 0 of 3,504,132 | 52 MB |
| nginx | 60,306 | 2.1 ms | 3.9 ms | 0 of 1,206,155 | 252 MB |
| Apache | 17,889 | 7.1 ms | 10.3 ms | 0 of 357,786 | 402 MB |

### PHP application, 17 KB page

| Server | Requests/s | p50 | p99 | Errors | Memory |
|---|--:|--:|--:|--:|--:|
| OpenLiteSpeed + LSAPI | 9,548 | 3.7 ms | 68.9 ms | 0 of 190,963 | 278 MB |
| OpenLiteSpeed + FastCGI | 9,538 | 6.1 ms | 60.4 ms | 0 of 190,771 | 316 MB |
| nginx + PHP-CGI | 2,949 | 43.2 ms | 48.4 ms | 0 of 58,998 | 251 MB |
| Apache + mod_php | 7,532 | 16.0 ms | 36.5 ms | 0 of 150,647 | 463 MB |
| Apache + PHP-CGI | 5,743 | 6.0 ms | 160 ms | 0 of 114,871 | 327 MB |

### WordPress home page, uncached, 113 KB

| Server | Requests/s | p50 | p99 | Errors | Memory |
|---|--:|--:|--:|--:|--:|
| OpenLiteSpeed + LSAPI | 117 | 420 ms | 1,667 ms | 0 of 2,348 | 218 MB |
| OpenLiteSpeed + FastCGI | 119 | 396 ms | 1,565 ms | 0 of 2,394 | 480 MB |
| nginx + PHP-CGI | 122 | 423 ms | 1,492 ms | 0 of 2,434 | 424 MB |
| Apache + mod_php | 105 | 574 ms | 1,229 ms | 0 of 2,110 | 929 MB |
| Apache + PHP-CGI | 118 | 425 ms | 1,621 ms | 0 of 2,372 | 500 MB |

## WordPress with a page cache on Windows

The same WordPress site with a full page cache, 20 second runs, PHP 8.4. OpenLiteSpeed uses its built-in cache through the LiteSpeed Cache plugin, installed and activated with no server changes. Apache uses `mod_cache_disk` and nginx `fastcgi_cache`, each set up to cache the page for an hour.

| Server and cache | Page | Connections | Requests/s | p50 | p99 | Errors |
|---|---|--:|--:|--:|--:|--:|
| OpenLiteSpeed + LiteSpeed Cache plugin | Home page | 64 | 21,180 | 2.9 ms | 6.1 ms | 0 of 423,617 |
| OpenLiteSpeed + LiteSpeed Cache plugin | Home page | 128 | 21,028 | 6.1 ms | 11.3 ms | 0 of 420,556 |
| OpenLiteSpeed + LiteSpeed Cache plugin | Post | 128 | 21,373 | 5.8 ms | 10.9 ms | 0 of 427,470 |
| Apache + mod_cache_disk | Home page | 64 | 18,362 | 3.5 ms | 6.1 ms | 0 of 367,256 |
| Apache + mod_cache_disk | Home page | 128 | 18,234 | 7.0 ms | 12.2 ms | 0 of 364,681 |
| nginx + fastcgi_cache | Home page | 64 | 7,745 | 8.1 ms | 10.8 ms | 0 of 154,906 |
| nginx + fastcgi_cache | Home page | 128 | 7,633 | 16.6 ms | 20.9 ms | 0 of 152,655 |
| nginx + fastcgi_cache | Post | 128 | 8,142 | 15.6 ms | 19.4 ms | 0 of 162,848 |

Without a cache the same home page runs at about 120 requests per second on every server. The LiteSpeed Cache plugin also knows when content changes: it tags every cached page and purges exactly the pages a new post or comment affects. The Apache and nginx caches in this test do not know about WordPress, so they serve the stored copy until it expires.

## Stress test on Windows

One continuous 60 second run with PHP 8.4. Requests per second for each 10 second window are computed from the start time of every request, so the load never pauses. The chart shows whether throughput holds up, the table whether memory grows.

### PHP application, 512 connections

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="bench-windows-stress-app-dark.svg">
  <img src="bench-windows-stress-app-light.svg" width="880" alt="Requests per second over 60 seconds for PHP application at 512 connections">
</picture>

| Server | Average requests/s | Slowest window | p99 | Errors | Memory first / last window |
|---|--:|--:|--:|--:|--:|
| OpenLiteSpeed + LSAPI | 9,434 | 9,146 | 107 ms | 0 of 566,022 | 304 / 306 MB |
| OpenLiteSpeed + FastCGI | 9,587 | 8,634 | 96.7 ms | 0 of 575,216 | 306 / 306 MB |
| nginx + PHP-CGI | 462 | 422 | 1,653 ms | 0 of 27,699 | 253 / 250 MB |
| Apache + mod_php | 3,078 | 2,322 | 586 ms | 0 of 184,708 | 1508 / 1508 MB |
| Apache + PHP-CGI | 5,396 | 5,262 | 600 ms | 0 of 323,765 | 388 / 409 MB |

## HTTPS on Windows

The PHP application over TLS, 10 second run, 128 keep-alive connections, PHP 8.4. Each keep-alive connection performs one TLS handshake.

| Server | Requests/s | p50 | p99 | Errors |
|---|--:|--:|--:|--:|
| OpenLiteSpeed + LSAPI | 8,687 | 11.5 ms | 46.9 ms | 0 of 86,872 |
| OpenLiteSpeed + FastCGI | 8,961 | 13.9 ms | 37.2 ms | 0 of 89,614 |
| nginx + PHP-CGI | 472 | 267 ms | 305 ms | 0 of 4,720 |
| Apache + mod_php | 4,473 | 27.9 ms | 38.2 ms | 0 of 44,746 |
| Apache + PHP-CGI | 4,356 | 28.9 ms | 35.7 ms | 0 of 43,563 |

## How the test was run

- Machine: AMD Ryzen 9 8940HX (16 cores, 32 threads), 48 GB RAM, Windows 11. Everything ran on one machine over 127.0.0.1.
- The web server and its PHP processes were pinned to the same 12 logical cores for every server, the load generator to 4 other cores and MySQL to another 4.
- Load: ApacheBench 2.3 (`ab -k`, and `abs` for HTTPS) in 4 processes sharing the connections. p50 and p99 come from the per-request percentiles of each process, weighted by its request count.
- Errors are failed requests plus responses other than 200, counted in every run.
- PHP: official builds from windows.php.net (8.2.35, 8.3.36, 8.4.27, 8.5.12) with the same `php.ini`: OPcache on, `mysqli`, `mbstring`, `openssl` and `fileinfo` loaded. Non thread safe builds everywhere except Apache with `mod_php`, which requires the thread safe build.
- OpenLiteSpeed for Windows 1.9.4 from the main branch: 6 HTTP workers and 24 PHP processes, either `lsphp.exe` over LSAPI or `php-cgi.exe` over FastCGI.
- nginx 1.30.6, official Windows build: one worker process, because the nginx documentation for Windows notes that only one worker actually does any work. `open_file_cache` on, 24 `php-cgi.exe` processes, a new FastCGI connection per request (`fastcgi_keep_conn` caused 30 second stalls with `php-cgi.exe`).
- Apache 2.4.54 (Apache Lounge build shipped with Laragon): `mpm_winnt` with 1024 threads, `AllowOverride None` everywhere. PHP through `mod_php`, or through `mod_proxy_fcgi` to the same 24 `php-cgi.exe` processes as nginx.
- PHP built-in server: `php -S`, which handles one request at a time and is meant for development only.
- WordPress 7.1.4 with the default theme, 31 posts with comments, MySQL 8.0.30 on the same machine. The uncached runs used no cache plugin, the page cache runs LiteSpeed Cache 7.9.2.
- HTTPS: TLS with a P-256 ECDSA certificate, session cache enabled on every server.
- Run lengths: 20 seconds per sustained run, one continuous 60 second run for the stress test and 10 seconds over HTTPS, each after a 2 second warm-up. The whole series takes under 10 minutes.
- Access logging was on for every server.

## Notes

- Uncached WordPress is limited by PHP and MySQL work, so every server lands at about the same speed. A page cache changes that picture, see above.
- nginx for Windows serves every connection from one worker process, and its documentation lists that as a known limitation. In these runs its throughput fell sharply at 512 connections and over HTTPS.
- When a client disconnects in the middle of a request, OpenLiteSpeed stops the `lsphp.exe` process that is working on it and starts a new one. Starting a process costs much more on Windows than on Linux. FastCGI keeps `php-cgi.exe` running in that case. An earlier version of this stress test restarted the load generator every 10 seconds, which cut off hundreds of requests at once and made LSAPI look uneven.
- Apache keeps one thread per connection. With 1024 threads it used about 1.6 GB of memory in the stress test.
- Earlier 5 second runs with PHP 8.2 to 8.5 gave the same order of servers.

## Limitations

- Client and server share one machine and talk over loopback, so network latency and bandwidth play no part.
- ApacheBench speaks HTTP/1.1 only. HTTP/2 and HTTP/3 were not measured.
- These numbers describe Windows. On Linux, nginx uses all cores with several workers and the gaps are different.
- Apache with `mod_php` on PHP 8.2 and 8.3 is slowed down by a global lock in thread safe PHP builds when floats are converted to strings. PHP 8.4 no longer has this bottleneck.

## Raw data

Every measurement is in [docs/bench-data](bench-data/) as CSV.
