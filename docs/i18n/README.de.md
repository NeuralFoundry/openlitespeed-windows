[English](../../README.md) | [Türkçe](README.tr.md) | [简体中文](README.zh-CN.md) | [Español](README.es.md) | [Português](README.pt-BR.md) | [Русский](README.ru.md) | [日本語](README.ja.md) | **Deutsch** | [Français](README.fr.md) | [한국어](README.ko.md) | [Bahasa Indonesia](README.id.md)

# OpenLiteSpeed for Windows

Website: [openlitespeed.dev/windows](https://openlitespeed.dev/windows/)

OpenLiteSpeed für Windows ist ein nativer Windows-Build des Webservers [OpenLiteSpeed](https://openlitespeed.org) 1.9.4. Er läuft unter Windows 10, Windows 11 und Windows Server ohne WSL, Docker oder virtuelle Maschine, führt PHP über LSAPI oder FastCGI aus und lässt sich eigenständig oder innerhalb von Laragon und XAMPP installieren. Die Full-Stack-Edition bringt zusätzlich ein Dashboard, MariaDB, phpMyAdmin und Cloudflare Tunnel mit, sodass eine Site auf Ihrem Computer auch hinter einer lokalen IP online gehen kann. Im Dashboard installieren Sie PHP- und Node.js-Versionen, geben jeder Site eine eigene PHP-Version, legen WordPress-, Laravel-, Node.js- und Python-Sites an und verwalten die Datenbanken. Kostenlos und Open Source unter GPL-3.0.

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../panel-dark.png">
  <img src="../panel-light.png" width="880" alt="Übersicht des Dashboards von OpenLiteSpeed für Windows mit Webserver, MariaDB, Cloudflare Tunnel, Live-Anfragezahlen und den lokalen Sites">
</picture>

## Funktionen

- Natives Win32-Programm, erstellt mit Visual C++, mit grafischem Installer, Tray-App und Windows-Dienst
- Zwei Editionen in einem Installer: nur der Webserver oder der Full Stack mit Dashboard, MariaDB, phpMyAdmin und Cloudflare Tunnel
- Dashboard: ein Fenster, um die Dienste zu starten und zu stoppen, Sites anzulegen, PHP und Node.js zu installieren, Datenbanken zu verwalten, Logs zu lesen, Ports zu ändern und Sites online zu stellen
- PHP 7.4 bis 8.5 und Node.js-LTS-Versionen werden aus dem Dashboard installiert und gegen ihre veröffentlichte SHA-256 geprüft
- Eine PHP-Version pro Site, PHP-Erweiterungen per Schalter ein- und ausschalten und die gängigen `php.ini`-Einstellungen in einem Formular
- Neue Sites als einfache PHP-Site, WordPress, Laravel (mit Composer), Node.js-App oder Python-App
- Datenbanken: eine Datenbank mit eigenem Benutzer anlegen, `.sql`-Dateien exportieren und importieren, eine Datenbank entfernen
- Eine lokale Site mit Cloudflare Tunnel online stellen: eine öffentliche https-Adresse für eine Site auf Ihrem Computer, ohne Portweiterleitung und ohne öffentliche IP. Eine eigene Domain funktioniert mit einem Tunnel-Token.
- MariaDB 11.8 und phpMyAdmin 5.2 nur auf 127.0.0.1, mit einem generierten root-Passwort. Jede neue WordPress-Site erhält eine eigene Datenbank und einen eigenen Benutzer.
- Nutzt alle CPU-Kerne: bis zu 32 HTTP-Worker-Prozesse auf denselben Ports, nach einem Absturz automatisch neu gestartet, mit Reload ohne Ausfallzeit
- PHP 8.2, 8.3, 8.4 und 8.5 über LSAPI (`lsphp.exe` für Windows ist enthalten) oder FastCGI (`php-cgi.exe`)
- Findet installierte PHP-Versionen, Laragon und XAMPP und konfiguriert sich dafür
- Importiert Apache-Sites aus `httpd.conf`: virtuelle Hosts, SSL-Zertifikate, Weiterleitungen, Aliase, Proxys, CGI, Zugriffsregeln und Passwortschutz, einschließlich der Regeln in `.htaccess`-Dateien
- CGI-Programme: `.exe`-, `.bat`- und `.cmd`-Dateien sowie Skripte mit einer `#!`-Zeile, etwa Perl oder Python, jeweils in einem eigenen Auftragsobjekt (Job Object) mit eingeschränktem Token
- Node.js- und Python-Anwendungen (WSGI), die der Server startet und neu startet. Node.js-Apps laufen unverändert über [openlitespeed-node](https://github.com/NeuralFoundry/openlitespeed-node), WebSockets eingeschlossen, siehe [docs/APPS.md](../APPS.md)
- Lokale Entwicklungs-Sites: Jeder Ordner im Projektordner wird zu `http://folder.test/`. Neue Sites, wahlweise mit WordPress, aus der Tray-App oder dem Dashboard
- Vertrauenswürdiges HTTPS für lokale Sites: eine auf Ihrem Computer erzeugte Zertifizierungsstelle, die nur `.test`-, `.localhost`- und `.internal`-Namen und die eigene Adresse des Computers signieren kann
- Proxy-Sites für Apps, die auf einem lokalen Port laufen, etwa `api.test` für eine App auf `127.0.0.1:3000`
- Signierte Updates: Die Tray-App lädt ein neues Release herunter, prüft seine Signatur und installiert es nach Ihrer Bestätigung
- HTTP/2, HTTP/3 (QUIC), TLS 1.3, Brotli, LSCache und die WebAdmin-Konsole
- Kostenlose HTTPS-Zertifikate von Let's Encrypt, die ein integrierter ACME-Client automatisch anfordert und erneuert, siehe [docs/ACME.md](../ACME.md)
- Integrierte Module: LSCache, Upload-Fortschritt, Parsen des Request-Bodys und Lua-Skripte mit LuaJIT, siehe [docs/MODULES.md](../MODULES.md)
- `.htaccess`-Dateien funktionieren wie bei Apache: Rewrite-Regeln, Zugriffsregeln (`Require`, `Order`, `Allow`, `Deny`), Passwörter, `<Files>`-Abschnitte, `Options Indexes` und `php_flag engine off`, innerhalb einer Sekunde nach einer Dateiänderung wirksam
- Web Application Firewall: ModSecurity 3 mit dem OWASP Core Rule Set, mit einem Befehl aktiviert, siehe [docs/PROTECTION.md](../PROTECTION.md#web-application-firewall)
- Bot-Schutz: reCAPTCHA- oder hCaptcha-Abfragen, Limits für Anfragen, Bandbreite und Verbindungen pro Client-IP sowie das Land des Besuchers aus MaxMind-GeoIP2- oder IP2Location-Datenbanken für PHP und Rewrite-Regeln, siehe [docs/PROTECTION.md](../PROTECTION.md)
- LiteSpeed Cache für WordPress funktioniert sofort: Plugin installieren und aktivieren, keine Serverkonfiguration nötig. Seiten werden bei Inhaltsänderungen automatisch aus dem Cache entfernt.
- Für Windows gehärtet: Dateinamen werden auf Tricks mit Groß- und Kleinschreibung, Kurznamen, Datenströmen und Gerätenamen geprüft, PHP läuft mit eingeschränktem Token und der Dienst unter einem virtuellen Konto

## Leistung: OpenLiteSpeed für Windows im Vergleich mit nginx und Apache

Der native Windows-Build aus diesem Repository im Vergleich mit nginx und Apache auf demselben Windows-11-Rechner, mit PHP 8.4 und denselben 12 CPU-Kernen für jeden Server. Anfragen pro Sekunde, höher ist besser. Alle Läufe endeten ohne Fehler.

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../bench-windows-overview-dark.svg">
  <img src="../bench-windows-overview-light.svg" width="880" alt="OpenLiteSpeed für Windows im Vergleich mit nginx und Apache: Anfragen pro Sekunde für eine statische Datei, eine PHP-Anwendung und WordPress">
</picture>

| Server | Statische Datei | PHP-Anwendung | WordPress, ungecacht | PHP über HTTPS | PHP bei 512 Verbindungen |
|---|--:|--:|--:|--:|--:|
| OpenLiteSpeed + LSAPI | 175.203 | 9.548 | 117 | 8.687 | 9.434 |
| OpenLiteSpeed + FastCGI |  | 9.538 | 119 | 8.961 | 9.587 |
| nginx + PHP-CGI | 60.306 | 2.949 | 122 | 472 | 462 |
| Apache + mod_php | 17.889 | 7.532 | 105 | 4.473 | 3.078 |
| Apache + PHP-CGI |  | 5.743 | 118 | 4.356 | 5.396 |

Statische Dateien betreffen PHP nicht, daher wurden sie einmal pro Server gemessen. Statische Datei, PHP-Anwendung und WordPress: Läufe über 20 Sekunden mit 128 Keep-Alive-Verbindungen, 64 für WordPress. HTTPS: 10 Sekunden, 128 Verbindungen. Die letzte Spalte ist der Durchschnitt eines durchgehenden 60-Sekunden-Lasttests mit 512 Verbindungen:

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../bench-windows-stress-app-dark.svg">
  <img src="../bench-windows-stress-app-light.svg" width="880" alt="Lasttest von OpenLiteSpeed für Windows: Anfragen pro Sekunde alle 10 Sekunden über 60 Sekunden mit 512 Verbindungen">
</picture>

Ungecachtes WordPress läuft auf jedem Server etwa gleich schnell, weil PHP und MySQL den Großteil der Arbeit erledigen. Mit einem Seitencache ändert sich das Bild. Die Startseite derselben Site, 64 Verbindungen:

| Server und Cache | Anfragen/s | p99 |
|---|--:|--:|
| OpenLiteSpeed + LiteSpeed-Cache-Plugin | 21.180 | 6,1 ms |
| Apache + mod_cache_disk | 18.362 | 6,1 ms |
| nginx + fastcgi_cache | 7.745 | 10,8 ms |

Latenz, Speicherverbrauch, der vollständige Aufbau und die Rohdaten stehen in [docs/BENCHMARKS.md](../BENCHMARKS.md).

## Voraussetzungen

- Windows 10, Windows 11 oder Windows Server 2019 oder neuer, x64
- PHP 8.2 oder neuer für Windows, x64. Der Installer kann es herunterladen.

## Installation

Laden Sie `OpenLiteSpeed-1.9.4-win64-setup.exe` unter [Releases](https://github.com/NeuralFoundry/openlitespeed-windows/releases/latest) herunter und führen Sie die Datei aus. Das Setup zeigt die gefundenen PHP-Versionen an, schlägt die Installationsart vor und fragt nach der Edition.

<img src="../setup.png" width="576" alt="Setup von OpenLiteSpeed für Windows mit der Auswahl der Edition und der PHP-Version">

| Edition | Enthält |
|---|---|
| Full Stack | OpenLiteSpeed, PHP, die Tray-App, das Dashboard, MariaDB, phpMyAdmin und Cloudflare Tunnel. Das Setup lädt etwa 170 MB herunter und prüft jede Datei gegen eine festgelegte SHA-256. |
| Nur Webserver | OpenLiteSpeed, PHP und die Tray-App mit neuen Sites, vertrauenswürdigem HTTPS, PHP-Wechsel, Importen und Updates |

Wenn Sie das Setup erneut ausführen, können Sie die Edition wechseln. Beim Wechsel zu „Nur Webserver“ bleiben Ihre Datenbanken erhalten. Die Full-Stack-Edition läuft mit der Tray-App, daher ist die Option für den Windows-Dienst nur für die Edition „Nur Webserver“ vorgesehen.

Das Setup beschränkt den Installationsordner auf Ihr Konto, die Administratoren und das System, sodass andere Benutzer des Computers die Programme und die Konfiguration nicht ändern können. Die Ordner von Laragon und XAMPP ändert es nicht, und deren PHP-Ordner können andere Benutzer oft ändern; mit dem Windows-Dienst können sie dadurch Code unter dem Dienstkonto ausführen, daher warnt die Dienstinstallation vor solchen PHP-Ordnern.

| Installationsart | Installationsordner | Projektordner | Gestartet durch |
|---|---|---|---|
| Eigenständiger Server | `C:\OpenLiteSpeed` | `C:\OpenLiteSpeed\www` | Tray-App oder Windows-Dienst |
| Innerhalb von Laragon | `C:\laragon\bin\openlitespeed` | Laragon-Dokumentstamm | Laragon **Start All** |
| Innerhalb von XAMPP | `C:\xampp\openlitespeed` | `C:\xampp\htdocs` | Tray-App oder Windows-Dienst |

Die Website antwortet auf Port 80 oder auf 8088, wenn Port 80 belegt ist. WebAdmin ist unter `https://localhost:7080/` erreichbar, mit dem Benutzer `admin` und dem Passwort, das am Ende des Setups angezeigt wird. Wenn Sie das Setup auf einer bestehenden Installation erneut ausführen, wird sie aktualisiert und die Konfiguration bleibt erhalten.

<img src="../webadmin.png" width="880" alt="OpenLiteSpeed-WebAdmin-Konsole unter Windows">

Unbeaufsichtigte Installation:

```bat
OpenLiteSpeed-1.9.4-win64-setup.exe --silent --mode standalone --download-php 8.4 --edition full
```

| Option | Beschreibung |
|---|---|
| `--mode standalone\|laragon\|xampp` | Installationsart, wird erkannt, wenn nicht angegeben |
| `--edition server\|full` | Edition; ohne Angabe `server` bei einer Neuinstallation und die aktuelle bei einem Upgrade. `--service` erfordert `server` |
| `--php <folder>` / `--download-php 8.2\|8.3\|8.4\|8.5` | Zu verwendendes PHP |
| `--dir <folder>` / `--www <folder>` | Installationsordner / Projektordner |
| `--port <n>` / `--admin-port <n>` | Ports für Website und WebAdmin |
| `--service` / `--no-tray` | Windows-Dienst installieren / Tray-App nicht starten |
| `--log <file>` | Logdatei, standardmäßig `%TEMP%\OpenLiteSpeed-setup.log`, enthält das Admin-Passwort |

Dieselben Dateien gibt es als ZIP mit `install.ps1` für skriptgesteuerte Bereitstellungen. Wie das Setup beschränkt das Skript den Installationsordner auf Ihr Konto, die Administratoren und das System, wenn der Ordner seine Berechtigungen noch vom übergeordneten Ordner erbt:

```powershell
powershell -ExecutionPolicy Bypass -File install.ps1 -Mode standalone -DownloadPhp 8.4 -Edition full
```

Zum Deinstallieren verwenden Sie **Einstellungen > Apps** oder führen `uninstall.exe --uninstall --silent` im Installationsordner aus. Der Projektordner und der Ordner `data` mit den Datenbanken bleiben erhalten.

## Dashboard

In der Full-Stack-Edition öffnet sich das Dashboard über das Startmenü oder per Doppelklick auf das Tray-Symbol. Es ist ein Desktopfenster und keine Webseite, daher lauscht es auf keinem Port. Die Seiten:

| Seite | Was Sie tun können |
|---|---|
| Overview | Webserver, MariaDB und Cloudflare Tunnel starten und stoppen, Anfragen pro Sekunde und offene Verbindungen sehen |
| Sites | Sites anlegen und entfernen, die PHP- oder Node.js-Version jeder Site wählen, eine Site online stellen, ihren Ordner, ein Terminal oder VS Code darin öffnen, einen Ordner als Node.js- oder Python-App ausführen, Hintergrundjobs ausführen |
| PHP | PHP-Versionen installieren, die Standardversion wählen, Erweiterungen und Xdebug ein- und ausschalten, `php.ini`-Einstellungen ändern, Composer installieren |
| Node.js | Node.js-Versionen installieren, die Version für die Apps wählen, eine App neu starten und ihr Log lesen |
| Databases | Eine Datenbank mit Benutzer anlegen, `.sql`-Dateien exportieren und importieren, eine Datenbank entfernen, phpMyAdmin öffnen |
| Mail | Den Mail-Catcher ein- und ausschalten, abgefangene Mails mit HTML-Teil und Anhängen lesen |
| Logs | Die Logs von Server, MariaDB, Apps und Tunnel lesen, Zeilen filtern, neue Zeilen live verfolgen |
| Settings | Start mit Windows, der Projektordner, vertrauenswürdiges HTTPS, die HTTP- und WebAdmin-Ports und das WebAdmin-Passwort |

**Terminal** unten links öffnet eine Eingabeaufforderung mit dem Standard-PHP, Composer, Node.js und dem MariaDB-Client im `PATH`.

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../panel-sites-dark.png">
  <img src="../panel-sites-light.png" width="880" alt="Seite Sites des Dashboards mit einer PHP-Version pro Site, Node.js- und Python-Apps, einer Proxy-Site und den Online-Schaltern">
</picture>

Das Dashboard benötigt die Microsoft Edge WebView2 Runtime, die in Windows 11 und aktuellen Versionen von Windows 10 enthalten ist.

## Lokale Sites

Jeder Ordner im Projektordner ist eine Site unter `http://folder.test/`. **New site** im Dashboard oder in der Tray-App legt den Ordner an. Das Dashboard bietet diese Arten von Sites:

| Art | Was Sie erhalten |
|---|---|
| PHP-Site | Einen Ordner mit einer `index.php` |
| WordPress | Das neueste WordPress. Wenn MariaDB installiert ist, eine Datenbank und einen Benutzer für die Site und eine ausgefüllte `wp-config.php`, sodass das WordPress-Setup nur noch nach dem Titel der Site und einem Administratorkonto fragt |
| Laravel | Ein neues Laravel-Projekt aus `composer create-project`, ausgeliefert aus seinem Ordner `public` |
| Node.js-App | Eine kleine `server.js` und `package.json`, vom Server mit dem Standard-Node.js gestartet |
| Python-App | Eine WSGI-Datei `wsgi.py`, vom Server mit dem auf dem Computer gefundenen Python gestartet |

Der WordPress-Installer lässt sich nur von diesem Computer aus öffnen, damit niemand sonst im Netzwerk die Installation vor Ihnen abschließen kann. Ein Ordner mit einer `package.json` wird mit **More > Run as a Node.js app** zu einer Node.js-App, **Run as a PHP site** macht das rückgängig. **Remove site** verschiebt den Ordner in den Papierkorb und behält die Datenbanken.

**New proxy site** gibt einer App, die auf einem lokalen Port lauscht, einen eigenen Namen, zum Beispiel `app.test` für einen Vite-Entwicklungsserver auf Port 5173. WebSockets werden durchgereicht. Eine Proxy-Site antwortet, wie die App dahinter, nur diesem Computer.

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../panel-site-dark.png">
  <img src="../panel-site-light.png" width="420" alt="Dialog New site mit dem Namen der Site und den Arten von Sites: PHP, WordPress, Laravel, Node.js-App und Python-App">
</picture>
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../panel-proxy-dark.png">
  <img src="../panel-proxy-light.png" width="420" alt="Dialog New proxy site mit dem Namen der Site und dem Port der App">
</picture>

## Eine Site online stellen

Schalten Sie **Online** neben einer Site im Dashboard ein oder führen Sie `openlitespeed --tunnel start <site>` aus. Cloudflare gibt der Site eine zufällige Adresse `https://*.trycloudflare.com`, die jeder öffnen kann, bis Sie sie wieder abschalten. Das funktioniert hinter NAT, einem Router oder einer lokalen IP, weil die Verbindung von Ihrem Computer nach außen aufgebaut wird, und in der Firewall wird nichts geöffnet. Besucher behalten ihre eigene IP-Adresse in den Logs und in PHP. Die öffentliche Adresse wird unter der lokalen angezeigt, daneben **Copy**.

Für eine eigene Domain legen Sie in Cloudflare Zero Trust einen Tunnel an, richten seinen öffentlichen Hostnamen auf `http://127.0.0.1` am Website-Port mit dem Namen der Site als HTTP-Host-Header aus und fügen das Tunnel-Token im Dashboard unter **Use your own domain** ein. Das Token wird verschlüsselt für Ihr Windows-Konto gespeichert.

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../panel-share-dark.png">
  <img src="../panel-share-light.png" width="420" alt="Dialog zum Freigeben einer lokalen Site im Internet über Cloudflare">
</picture>
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../panel-token-dark.png">
  <img src="../panel-token-light.png" width="420" alt="Dialog zum Verbinden einer eigenen Domain mit einem Cloudflare-Tunnel-Token">
</picture>

Nur Sites aus dem Projektordner und Proxy-Sites können online gehen. Beim Freigeben einer Proxy-Site verbindet sich der Tunnel direkt mit ihrer App; bei einer eigenen Domain richten Sie diesen Hostnamen ebenfalls auf den Port der App aus. phpMyAdmin und WebAdmin antworten nur diesem Computer, und ein Tunnel startet nicht, wenn der Server seine Besucher nicht von diesem Computer unterscheiden kann.

## Datenbanken

MariaDB lauscht nur auf `127.0.0.1` und läuft unter Ihrem Konto, nie als Administrator. Das Setup lädt MariaDB herunter, und die Tray-App legt die Datenbanken beim ersten Start an. Das root-Passwort wird dabei generiert und in `data\client.cnf` gespeichert, das nur Ihr Konto und die Administratoren lesen können. `SELECT ... INTO OUTFILE` und `LOAD DATA` funktionieren nur in `data\files`. **Copy password** im Dashboard oder in der Tray-App legt das Passwort in die Zwischenablage, ohne es zum Zwischenablageverlauf hinzuzufügen. phpMyAdmin ist unter `http://phpmyadmin.test/` erreichbar, nur von diesem Computer aus.

Eine WordPress-Site, die über die Tray-App oder das Dashboard angelegt wird, erhält eine eigene Datenbank und einen eigenen Benutzer, und `wp-config.php` wird ausgefüllt.

Auf der Seite **Databases** legt **New database** eine Datenbank mit einem gleichnamigen Benutzer und einem generierten Passwort an, das einmalig mit einer Schaltfläche **Copy** angezeigt wird. **Export** schreibt mit `mariadb-dump` eine `.sql`-Datei, **Import** lädt eine `.sql`-Datei in die Datenbank, und **Remove** verlangt zuerst die Eingabe des Namens und kann auch den Benutzer entfernen.

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../panel-db-dark.png">
  <img src="../panel-db-light.png" width="880" alt="Seite Databases des Dashboards mit MariaDB, dem root-Passwort sowie Export, Import und Entfernen für jede Datenbank">
</picture>

## Vertrauenswürdiges HTTPS

**Trusted HTTPS** im Dashboard oder in der Tray-App oder `openlitespeed --dev-https on` erzeugt eine Zertifizierungsstelle auf Ihrem Computer, bittet Windows, ihr für Ihr Konto zu vertrauen, und liefert jede lokale Site unter `https://name.test/` aus. Die Zertifizierungsstelle ist durch Namensbeschränkungen (Name Constraints) auf `.test`-, `.localhost`- und `.internal`-Namen sowie auf `127.0.0.1` und `::1` begrenzt und kann daher für keine andere Site ein Zertifikat signieren. Beim Abschalten wird sie aus dem Windows-Zertifikatspeicher entfernt.

## PHP

`lsphp.exe` ist die für Windows erstellte PHP-LiteSpeed-SAPI für PHP 8.2 bis 8.5 in den Varianten Thread Safe und Non Thread Safe. Sie läuft auf den offiziellen PHP-Builds von [windows.php.net](https://windows.php.net), daher funktionieren vorhandene Erweiterungen und `php.ini`-Einstellungen weiter. Wenn eine PHP-Version sie nicht laden kann, weicht OpenLiteSpeed auf `php-cgi.exe` über FastCGI aus.

<img src="../phpinfo.png" width="640" alt="phpinfo von PHP 8.4 unter Windows mit Server API LiteSpeed">

Wechseln Sie die PHP-Version im Dashboard, im Tray-Menü oder mit `openlitespeed --set-php <folder>`.

**Install PHP** auf der Seite PHP listet die Versionen von windows.php.net auf, von 7.4 bis 8.5, Thread Safe und Non Thread Safe. Ein Download wird gegen die SHA-256 geprüft, die windows.php.net veröffentlicht, nach `php\<version>` entpackt und erhält eine `php.ini` mit den üblichen Erweiterungen aktiviert. Versionen ab 8.2 laufen über LSAPI, ältere über FastCGI.

Jede Site kann ihre eigene PHP-Version verwenden: Wählen Sie sie in der Spalte PHP der Seite Sites oder führen Sie `openlitespeed --site php <site> <folder>` aus. Sites ohne Auswahl verwenden die Standardversion. Für jede Version enthält das Menü **More** den Eintrag **Extensions** mit einem Schalter für jede Erweiterung dieses PHP und **Settings** für Speicherlimit, Upload- und POST-Größe, Ausführungszeit, Eingabevariablen, Fehleranzeige und Zeitzone. **Edit php.ini** öffnet die Datei im Editor. Änderungen laden den Webserver neu.

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../panel-php-dark.png">
  <img src="../panel-php-light.png" width="880" alt="Seite PHP des Dashboards mit drei PHP-Versionen, der Standardversion, den Sites, die jede Version verwenden, und Composer">
</picture>

**Install** unter Composer lädt `composer.phar` von getcomposer.org herunter und prüft es gegen seine SHA-256. Im Terminal des Dashboards liegt `composer` im `PATH`, und Laravel-Sites werden damit angelegt.

## Xdebug

Der Schalter in der Spalte Xdebug der Seite PHP oder `openlitespeed --php xdebug <folder> on` schaltet Xdebug für diese PHP-Version ein. Ein Xdebug, das bereits aus dem Ordner `ext` geladen wird, wird verwendet. Andernfalls wird der offizielle Windows-Build für diese PHP-Version von xdebug.org heruntergeladen und gegen eine in OpenLiteSpeed hinterlegte SHA-256 geprüft: Xdebug 3.5.3 für PHP 8.2 bis 8.5, Thread Safe und Non Thread Safe.

Xdebug startet mit jeder Anfrage und verbindet sich mit einem Debugger auf `127.0.0.1:9003`. Ohne Debugger läuft die Anfrage sofort weiter, und in die Logs wird nichts geschrieben. Der opcache-JIT ist ausgeschaltet, solange Xdebug aktiv ist, und kehrt zurück, wenn Xdebug abgeschaltet wird. Die Einstellungen stehen am Ende der `php.ini`, und der Webserver wird neu geladen, damit sie wirksam werden.

Xdebug lässt sich nicht einschalten, solange OpenLiteSpeed als Windows-Dienst installiert ist. Jedes Programm auf dem Computer, das auf Port 9003 lauscht, erhält die Debug-Sitzung und kann PHP-Code unter dem Dienstkonto ausführen, schalten Sie Xdebug daher aus, bevor Sie den Dienst installieren.

**VS Code:** **More > Open in VS Code** auf der Seite Sites öffnet den Ordner der Site in VS Code, wenn es installiert ist. Starten Sie eine PHP-Debug-Sitzung, die auf Port 9003 lauscht, etwa mit der Erweiterung PHP Debug, und öffnen Sie die Site im Browser.

## Mail-Catcher

Der Schalter auf der Seite Mail oder `openlitespeed --mail on` fängt jede Mail ab, die die lokalen Sites senden, sodass nichts den Computer verlässt. PHP `mail()` gelangt über `sendmail_path` in der `php.ini` der PHP-Versionen, die OpenLiteSpeed ausführt, zu OpenLiteSpeed, und Apps, die über SMTP senden, verwenden den Host `127.0.0.1`, Port 1025, keine Verschlüsselung und einen beliebigen Benutzernamen und ein beliebiges Passwort. Neue Laravel-Sites erhalten diese Einstellungen in `.env`. Eine Verbindung, die wie eine Webanfrage beginnt, wird geschlossen, sodass eine Webseite im Browser keine Mails an den Mail-Catcher senden kann.

Die Mails werden in `data\mail` gespeichert, das nur Ihr Konto lesen kann, bis zu den neuesten 1000 Nachrichten und 512 MB. Die Seite Mail zeigt jede Nachricht mit ihrem Text- und HTML-Teil an und ermöglicht das Speichern der Anhänge. Der HTML-Teil wird ohne Skripte dargestellt und lädt nichts aus dem Internet.

## Hintergrundjobs

**Background jobs** im Menü **More** einer Site oder `openlitespeed --job add <site> <name> <command>` führt einen lang laufenden Befehl wie `php artisan queue:work`, `php artisan schedule:work` oder `npm run dev` im Ordner der Site aus, mit den PHP- und Node.js-Versionen dieser Site an erster Stelle im `PATH`. Jobs starten und stoppen mit dem Webserver. Ein beendeter Job wird nach 1, 2, 4 und bis zu 60 Sekunden erneut gestartet, und nach 10 Neustarts in 10 Minuten bleibt er gestoppt. Das Stoppen eines Jobs beendet alle Prozesse, die er gestartet hat. Die Ausgabe geht nach `logs\jobs\<site>.<name>.log`, das die Seite Logs anzeigt. Mit dem Windows-Dienst führt das Dienstkonto die Jobs und den Mail-Catcher aus, so wie es auch PHP ausführt.

## Node.js

**Install Node.js** auf der Seite Node.js listet die Current- und LTS-Versionen von nodejs.org auf. Ein Download wird gegen `SHASUMS256.txt` geprüft und nach `node\<version>` entpackt. Die Seite listet außerdem Node.js-Installationen auf, die in `Program Files`, Laragon, nvm, Volta und im `PATH` gefunden wurden. **Make default** wählt die Version, mit der die Node.js-Apps laufen, und die Apps werden darunter mit **Restart** und **Log** aufgeführt.

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../panel-node-dark.png">
  <img src="../panel-node-light.png" width="880" alt="Seite Node.js des Dashboards mit den installierten Node.js-Versionen und den laufenden Apps">
</picture>

Eine Node.js-App braucht keine Änderungen: `app.listen(3000)` wird auf die Adresse umgelenkt, die der Server ihr zuweist. Siehe [docs/APPS.md](../APPS.md).

Jede Site kann ihr eigenes Node.js verwenden: Wählen Sie es in der Spalte Version der Seite Sites oder führen Sie `openlitespeed --site node <site> <node.exe>` aus. Ohne Auswahl wählt eine Datei `.nvmrc` oder `.node-version`, die eine Hauptversion wie `20` nennt, das neueste installierte Node.js 20, andernfalls läuft die Site mit der Standardversion. Hintergrundjobs und das Terminal der Site verwenden dieselbe Version.

## Umstieg von Apache

OpenLiteSpeed liest eine vorhandene Apache-Konfiguration und legt für jeden `<VirtualHost>` eine Site an, mit ihren Domains, dem Dokumentstamm, SSL-Zertifikat, Weiterleitungen, Rewrite-Regeln, Aliasen, Proxys, Zugriffsregeln und Passwortschutz. Regeln in `.htaccess`-Dateien funktionieren weiter, weil OpenLiteSpeed sie selbst anwendet.

```bat
bin\openlitespeed.exe --import-apache --dry-run
bin\openlitespeed.exe --import-apache C:\Apache24\conf\httpd.conf
bin\openlitespeed.exe -s reload
```

Ohne Pfad wird die Apache-Konfiguration von Laragon, XAMPP, WampServer oder `C:\Apache24` automatisch gefunden. `--dry-run` gibt den Bericht aus, ohne etwas zu ändern. Ein erneuter Import ersetzt den vorherigen, und `--import-apache --remove` entfernt ihn. Regeln, die sich nicht umwandeln lassen, werden im Bericht aufgeführt. Zugriffsregeln werden exakt oder strenger umgewandelt, nie großzügiger; die einzige Ausnahme, Regeln, die bestimmte Clients anhand von Hostname oder Umgebungsvariable blockieren, wird als Warnung gemeldet. Details stehen in [docs/APACHE.md](../APACHE.md).

## Umstieg von nginx

`--import-nginx` macht dasselbe für nginx: Jeder `server`-Block wird zu einer Site mit ihren Domains, Listenern, SSL-Zertifikat, Locations, `try_files`, Rewrites, Weiterleitungen, Aliasen, Proxys, Zugriffsregeln und Passwortschutz, und PHP hinter `fastcgi_pass` läuft über das PHP von OpenLiteSpeed.

```bat
bin\openlitespeed.exe --import-nginx --dry-run
bin\openlitespeed.exe --import-nginx C:\nginx\conf\nginx.conf
```

Details stehen in [docs/NGINX.md](../NGINX.md).

## Mehrere CPU-Kerne

OpenLiteSpeed startet einen HTTP-Worker pro CPU-Kern, standardmäßig bis zu 8. Setzen Sie **Number of Workers** unter **Server Configuration > General** in WebAdmin oder `httpdWorkers` in `conf\httpd_config.conf` auf einen beliebigen Wert von 1 bis 32. Jeder Worker hat seinen eigenen PHP-Pool. Wie die Worker unter Windows die lauschenden Sockets teilen, beschreibt [docs/ARCHITECTURE.md](../ARCHITECTURE.md).

## Befehlszeile

Führen Sie die Befehle im Installationsordner als `bin\openlitespeed.exe` aus.

| Befehl | Beschreibung |
|---|---|
| `openlitespeed -s start\|stop\|restart\|reload\|status` | Den Server steuern, `reload` ist ein Neustart ohne Ausfallzeit |
| `openlitespeed -d` | Im Vordergrund ausführen |
| `openlitespeed --install-service` / `--uninstall-service` | Den Windows-Dienst registrieren oder entfernen |
| `openlitespeed --set-php <folder>` | Eine andere PHP-Version verwenden |
| `openlitespeed --import-apache [httpd.conf] [--dry-run]` / `--import-apache --remove` | Die Apache-Sites importieren / die importierten Sites entfernen |
| `openlitespeed --import-nginx [nginx.conf] [--dry-run]` / `--import-nginx --remove` | Die nginx-Sites importieren / die importierten Sites entfernen |
| `openlitespeed --acme-install [--email <address>]` | Automatische Let's-Encrypt-Zertifikate einschalten |
| `openlitespeed --modsec-install [--detection-only]` / `--modsec-install --remove` | Die ModSecurity-Firewall mit dem OWASP Core Rule Set ein- oder ausschalten |
| `openlitespeed --dev-sites <folder> [tld]` | Für jeden Unterordner eines Ordners eine Site anlegen |
| `openlitespeed --new-site <name> [--wordpress\|--laravel\|--node\|--python]` | Eine Site im Projektordner anlegen: eine PHP-Site, WordPress mit Datenbank, ein Laravel-Projekt, eine Node.js-App oder eine Python-App |
| `openlitespeed --site php <site> <folder>\|default` | Eine andere PHP-Version für eine Site verwenden |
| `openlitespeed --site app <site> node\|python\|off` | Einen Site-Ordner als Node.js- oder Python-App oder wieder als PHP-Site ausführen |
| `openlitespeed --site node <site> <node.exe>\|default` | Eine andere Node.js-Version für eine Site verwenden |
| `openlitespeed --php list` | Die PHP-Versionen auf diesem Computer als JSON, jeweils mit den Sites und dem Xdebug-Status |
| `openlitespeed --php available` / `install <8.x> [ts]` / `remove <folder>` | Die PHP-Versionen von windows.php.net auflisten und installieren, eine heruntergeladene entfernen |
| `openlitespeed --php ext <folder>` / `ext-set <folder> <name> on\|off` | Die Erweiterungen einer PHP-Version auflisten, eine ein- oder ausschalten |
| `openlitespeed --php ini <folder>` / `ini-set <folder> <key> <value>` | Eine gängige `php.ini`-Einstellung lesen oder ändern |
| `openlitespeed --php xdebug <folder> on\|off\|status` | Xdebug für eine PHP-Version ein- oder ausschalten oder anzeigen, ob es aktiv ist |
| `openlitespeed --node list\|available` / `install <major>` / `default <node.exe>` / `remove <node.exe>` | Die Node.js-Versionen für die Apps verwalten |
| `openlitespeed --composer install` | Composer nach `tools\composer` herunterladen |
| `openlitespeed --set-port http\|admin <port>` | Den Website- oder WebAdmin-Port ändern |
| `openlitespeed --proxy add <name> <port>` / `remove <name>` / `list` | Eine Proxy-Site für eine App auf einem lokalen Port hinzufügen oder entfernen |
| `openlitespeed --dev-https on [port]\|off\|status` | Vertrauenswürdiges HTTPS für die lokalen Sites |
| `openlitespeed --dev-hosts` / `--dev-hosts-clear` | Die Site-Namen in der hosts-Datei eintragen oder entfernen |
| `openlitespeed --db install\|fetch\|start\|stop\|status` | MariaDB und phpMyAdmin installieren, nur herunterladen, starten oder stoppen |
| `openlitespeed --db list` / `create <name> [--no-user]` / `drop <name> [--user]` | Datenbanken auflisten, anlegen oder entfernen |
| `openlitespeed --db export <name> <file.sql>` / `import <name> <file.sql>` | Eine Datenbank in eine Datei exportieren oder eine Datei in sie importieren |
| `openlitespeed --tunnel start <site>` / `stop <site>` / `stop --all` / `status` | Eine Site mit Cloudflare Tunnel online stellen oder offline nehmen |
| `openlitespeed --tunnel token` / `run` / `run-stop` / `token clear` | Ein Tunnel-Token von der Standardeingabe speichern, die eigene Domain verbinden oder trennen, das Token vergessen |
| `openlitespeed --mail on\|off\|status` | Den Mail-Catcher ein- oder ausschalten oder anzeigen, ob er läuft |
| `openlitespeed --mail list` / `remove <id>` / `save <id> <n> <file>` / `clear` | Die abgefangenen Mails als JSON auflisten, eine Nachricht entfernen, Anhang `n` (ab 0) einer Nachricht in eine Datei speichern oder alle Nachrichten entfernen |
| `openlitespeed --mail sendmail [-t] [-i] [-f <from>] [<recipient>...]` | Eine Nachricht von der Standardeingabe speichern, wie sendmail |
| `openlitespeed --job list` / `add <site> <name> <command>` / `remove <site> <name>` | Die Hintergrundjobs der Sites auflisten, hinzufügen oder entfernen |
| `openlitespeed --job start\|stop\|restart <site> <name>` / `autostart <site> <name> on\|off` | Einen Job steuern, festlegen, ob er mit dem Webserver startet |
| `openlitespeed --status` | Server, PHP, Sites, Datenbank und Tunnel als JSON |
| `openlitespeed --admin-password admin\conf\htpasswd <password>` | Das WebAdmin-Passwort setzen |

Die Tray-App startet, stoppt und lädt den Server neu, öffnet die Sites, WebAdmin und die Logs, legt Sites und Proxy-Sites an, schaltet vertrauenswürdiges HTTPS ein und aus, wechselt die PHP-Version und importiert Apache- und nginx-Sites. In der Full-Stack-Edition öffnet sie außerdem das Dashboard und steuert MariaDB. Einmal am Tag prüft sie auf GitHub, ob es ein neues Release gibt; **Update** lädt es herunter, prüft Signatur und Prüfsumme und führt das neue Setup aus. `check=0` unter `[update]` in `conf\olstray.ini` schaltet die Prüfung ab.

## Aus dem Quellcode bauen

Siehe [docs/BUILD.md](../BUILD.md). Der Build verwendet Visual Studio 2022, CMake und Ninja und lädt alle Abhängigkeiten in festgelegten Commits.

## Einschränkungen

- Kein Benutzerwechsel pro virtuellem Host (suEXEC). PHP läuft als der Benutzer, der den Server ausführt, oder unter dem Dienstkonto.
- Kein `SO_REUSEPORT`. Worker teilen sich einen lauschenden Socket pro Adresse.
- cgroups, Namespaces, bubblewrap und CageFS sind Linux-Funktionen und stehen nicht zur Verfügung.
- Ruby-Anwendungen über den App-Server-Kontext werden nicht unterstützt.

## Mitwirken

Fehlerberichte und Pull Requests sind willkommen, siehe [CONTRIBUTING.md](../../CONTRIBUTING.md). Melden Sie Sicherheitsprobleme vertraulich, wie in [SECURITY.md](../../SECURITY.md) beschrieben.

## Lizenz

GPL-3.0, siehe [LICENSE](../../LICENSE). OpenLiteSpeed wird von LiteSpeed Technologies entwickelt und steht unter der GPL-3.0. `lsphp.exe` wird aus den PHP-Quellen gebaut und unterliegt der [PHP License](../../win/lsphp/PHP-LICENSE.txt). Komponenten von Drittanbietern sind in [THIRD-PARTY-NOTICES.txt](../../win/package/THIRD-PARTY-NOTICES.txt) aufgeführt.

Dieses Projekt steht in keiner Verbindung zu LiteSpeed Technologies oder der PHP Group und wird von ihnen nicht unterstützt. LiteSpeed und OpenLiteSpeed sind Marken von LiteSpeed Technologies, Inc.

Ebenfalls vom selben Autor: [OpenLiteSpeed for cPanel and WHM](https://github.com/NeuralFoundry/openlitespeed-cpanel).
