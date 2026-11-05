[English](../../README.md) | [Türkçe](README.tr.md) | [简体中文](README.zh-CN.md) | [Español](README.es.md) | [Português](README.pt-BR.md) | [Русский](README.ru.md) | [日本語](README.ja.md) | [Deutsch](README.de.md) | [Français](README.fr.md) | [한국어](README.ko.md) | **Bahasa Indonesia**

# OpenLiteSpeed for Windows

Situs web: [openlitespeed.dev/windows](https://openlitespeed.dev/windows/)

OpenLiteSpeed untuk Windows adalah build native Windows dari web server [OpenLiteSpeed](https://openlitespeed.org) 1.9.4. Server ini berjalan di Windows 10, Windows 11, dan Windows Server tanpa WSL, Docker, atau mesin virtual, menjalankan PHP melalui LSAPI atau FastCGI, dan dapat diinstal sendiri atau di dalam Laragon dan XAMPP. Edisi full stack menambahkan dasbor, MariaDB, phpMyAdmin, dan Cloudflare Tunnel, sehingga situs di komputer Anda bisa online meskipun berada di balik IP lokal. Dari dasbor, Anda dapat menginstal versi PHP dan Node.js, memberi setiap situs versi PHP sendiri, membuat situs WordPress, Laravel, Node.js, dan Python, serta mengelola database. Gratis dan open source dengan lisensi GPL-3.0.

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../panel-dark.png">
  <img src="../panel-light.png" width="880" alt="Ringkasan dasbor OpenLiteSpeed untuk Windows dengan web server, MariaDB, Cloudflare Tunnel, jumlah request langsung, dan situs lokal">
</picture>

## Fitur

- Program Win32 native yang dibuat dengan Visual C++, dilengkapi installer grafis, aplikasi tray, dan layanan Windows
- Dua edisi dalam satu installer: hanya web server, atau full stack dengan dasbor, MariaDB, phpMyAdmin, dan Cloudflare Tunnel
- Dasbor: satu jendela untuk menjalankan dan menghentikan layanan, membuat situs, menginstal PHP dan Node.js, mengelola database, membaca log, mengubah port, dan membuat situs online
- PHP 7.4 hingga 8.5 dan versi LTS Node.js diinstal dari dasbor, diverifikasi dengan SHA-256 yang dipublikasikan
- Versi PHP untuk setiap situs, ekstensi PHP yang dinyalakan dan dimatikan dengan sakelar, dan pengaturan `php.ini` yang umum dalam sebuah formulir
- Situs baru berupa situs PHP biasa, WordPress, Laravel (dengan Composer), aplikasi Node.js, atau aplikasi Python
- Database: membuat database dengan user sendiri, mengekspor dan mengimpor file `.sql`, menghapus database
- Membuat situs lokal online dengan Cloudflare Tunnel: alamat https publik untuk situs di komputer Anda, tanpa port forwarding dan tanpa IP publik. Domain Anda sendiri bisa digunakan dengan tunnel token.
- MariaDB 11.8 dan phpMyAdmin 5.2 hanya di 127.0.0.1, dengan kata sandi root yang dibuat otomatis. Setiap situs WordPress baru mendapat database dan user sendiri.
- Memakai semua core CPU: hingga 32 proses worker HTTP pada port yang sama, dijalankan ulang otomatis setelah crash, dengan reload tanpa downtime
- PHP 8.2, 8.3, 8.4, dan 8.5 melalui LSAPI (`lsphp.exe` untuk Windows sudah disertakan) atau FastCGI (`php-cgi.exe`)
- Menemukan versi PHP yang terinstal, Laragon, dan XAMPP, lalu mengonfigurasi dirinya untuk semuanya
- Mengimpor situs Apache dari `httpd.conf`: virtual host, sertifikat SSL, redirect, alias, proxy, CGI, aturan akses, dan proteksi kata sandi, termasuk aturan di file `.htaccess`
- Program CGI: file `.exe`, `.bat`, dan `.cmd` serta skrip dengan baris `#!` seperti Perl atau Python, masing-masing di job object sendiri dengan token terbatas
- Aplikasi Node.js dan Python (WSGI) yang dijalankan dan dijalankan ulang oleh server. Aplikasi Node.js berjalan tanpa perubahan melalui [openlitespeed-node](https://github.com/NeuralFoundry/openlitespeed-node), termasuk WebSocket, lihat [docs/APPS.md](../APPS.md)
- Situs pengembangan lokal: setiap folder di folder proyek menjadi `http://folder.test/`. Situs baru, opsional dengan WordPress, dari tray atau dasbor
- HTTPS tepercaya untuk situs lokal: otoritas sertifikat yang dibuat di komputer Anda dan hanya dapat menandatangani nama `.test`, `.localhost`, dan `.internal` serta alamat komputer itu sendiri
- Situs proxy untuk aplikasi yang berjalan di port lokal, misalnya `api.test` untuk aplikasi di `127.0.0.1:3000`
- Pembaruan bertanda tangan: aplikasi tray mengunduh rilis baru, memeriksa tanda tangannya, dan menginstalnya setelah Anda konfirmasi
- HTTP/2, HTTP/3 (QUIC), TLS 1.3, Brotli, LSCache, dan konsol WebAdmin
- Sertifikat HTTPS gratis dari Let's Encrypt, diminta dan diperpanjang otomatis oleh klien ACME bawaan, lihat [docs/ACME.md](../ACME.md)
- Modul bawaan: LSCache, progres upload, parsing body request, dan skrip Lua dengan LuaJIT, lihat [docs/MODULES.md](../MODULES.md)
- File `.htaccess` bekerja seperti di Apache: aturan rewrite, aturan akses (`Require`, `Order`, `Allow`, `Deny`), kata sandi, bagian `<Files>`, `Options Indexes`, dan `php_flag engine off`, diterapkan dalam satu detik saat file berubah
- Web application firewall: ModSecurity 3 dengan OWASP Core Rule Set, diaktifkan dengan satu perintah, lihat [docs/PROTECTION.md](../PROTECTION.md#web-application-firewall)
- Proteksi bot: tantangan reCAPTCHA atau hCaptcha, batas request, bandwidth, dan koneksi per IP klien, serta negara pengunjung dari database MaxMind GeoIP2 atau IP2Location untuk PHP dan aturan rewrite, lihat [docs/PROTECTION.md](../PROTECTION.md)
- LiteSpeed Cache untuk WordPress langsung berfungsi: instal dan aktifkan plugin, tanpa konfigurasi server. Halaman otomatis dihapus dari cache saat konten berubah.
- Diperkuat untuk Windows: nama file diperiksa terhadap trik huruf besar kecil, nama pendek, stream, dan nama perangkat, PHP berjalan dengan token terbatas, dan layanan berjalan dengan akun virtual

## Performa: OpenLiteSpeed untuk Windows vs nginx dan Apache

Build native Windows dari repositori ini dibandingkan dengan nginx dan Apache di mesin Windows 11 yang sama, dengan PHP 8.4 dan 12 core CPU yang sama untuk setiap server. Request per detik, makin tinggi makin baik. Semua pengujian selesai tanpa error.

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../bench-windows-overview-dark.svg">
  <img src="../bench-windows-overview-light.svg" width="880" alt="OpenLiteSpeed untuk Windows vs nginx dan Apache: request per detik untuk file statis, aplikasi PHP, dan WordPress">
</picture>

| Server | File statis | Aplikasi PHP | WordPress, tanpa cache | PHP lewat HTTPS | PHP pada 512 koneksi |
|---|--:|--:|--:|--:|--:|
| OpenLiteSpeed + LSAPI | 175.203 | 9.548 | 117 | 8.687 | 9.434 |
| OpenLiteSpeed + FastCGI |  | 9.538 | 119 | 8.961 | 9.587 |
| nginx + PHP-CGI | 60.306 | 2.949 | 122 | 472 | 462 |
| Apache + mod_php | 17.889 | 7.532 | 105 | 4.473 | 3.078 |
| Apache + PHP-CGI |  | 5.743 | 118 | 4.356 | 5.396 |

File statis tidak melibatkan PHP, jadi diukur sekali per server. File statis, aplikasi PHP, dan WordPress: pengujian 20 detik dengan 128 koneksi keep-alive, 64 untuk WordPress. HTTPS: 10 detik, 128 koneksi. Kolom terakhir adalah rata-rata dari stress test 60 detik tanpa henti dengan 512 koneksi:

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../bench-windows-stress-app-dark.svg">
  <img src="../bench-windows-stress-app-light.svg" width="880" alt="Stress test OpenLiteSpeed untuk Windows: request per detik setiap 10 detik selama 60 detik dengan 512 koneksi">
</picture>

WordPress tanpa cache berjalan dengan kecepatan yang hampir sama di setiap server karena PHP dan MySQL mengerjakan sebagian besar pekerjaan. Dengan page cache, hasilnya berubah. Halaman beranda dari situs yang sama, 64 koneksi:

| Server dan cache | Request/detik | p99 |
|---|--:|--:|
| OpenLiteSpeed + plugin LiteSpeed Cache | 21.180 | 6,1 ms |
| Apache + mod_cache_disk | 18.362 | 6,1 ms |
| nginx + fastcgi_cache | 7.745 | 10,8 ms |

Latensi, memori, pengaturan lengkap, dan data mentah ada di [docs/BENCHMARKS.md](../BENCHMARKS.md).

## Persyaratan

- Windows 10, Windows 11, atau Windows Server 2019 atau yang lebih baru, x64
- PHP 8.2 atau yang lebih baru untuk Windows, x64. Installer dapat mengunduhnya.

## Instalasi

Unduh `OpenLiteSpeed-1.9.4-win64-setup.exe` dari [Releases](https://github.com/NeuralFoundry/openlitespeed-windows/releases/latest), lalu jalankan. Setup menampilkan versi PHP yang ditemukan, menyarankan jenis setup, dan menanyakan edisi yang akan diinstal.

<img src="../setup.png" width="576" alt="Setup OpenLiteSpeed untuk Windows dengan pilihan edisi dan versi PHP">

| Edisi | Isi |
|---|---|
| Full stack | OpenLiteSpeed, PHP, aplikasi tray, dasbor, MariaDB, phpMyAdmin, dan Cloudflare Tunnel. Setup mengunduh sekitar 170 MB dan memeriksa setiap file dengan SHA-256 yang dipatok. |
| Hanya web server | OpenLiteSpeed, PHP, dan aplikasi tray dengan situs baru, HTTPS tepercaya, penggantian versi PHP, impor, dan pembaruan |

Menjalankan setup lagi akan mengganti edisi. Beralih ke edisi hanya web server tetap mempertahankan database Anda. Edisi full stack berjalan bersama aplikasi tray, jadi opsi layanan Windows hanya untuk edisi hanya web server.

Setup membatasi folder instalasi hanya untuk akun Anda, administrator, dan sistem, sehingga pengguna lain di komputer tersebut tidak dapat mengubah program atau konfigurasinya. Setup tidak mengubah folder Laragon dan XAMPP, yang folder PHP-nya sering dapat diubah oleh pengguna lain; dengan layanan Windows, hal itu memungkinkan mereka menjalankan kode sebagai akun layanan, sehingga instalasi layanan memberi peringatan tentang folder PHP seperti itu.

| Jenis setup | Folder instalasi | Folder proyek | Dijalankan oleh |
|---|---|---|---|
| Server mandiri | `C:\OpenLiteSpeed` | `C:\OpenLiteSpeed\www` | Aplikasi tray atau layanan Windows |
| Di dalam Laragon | `C:\laragon\bin\openlitespeed` | Document root Laragon | Laragon **Start All** |
| Di dalam XAMPP | `C:\xampp\openlitespeed` | `C:\xampp\htdocs` | Aplikasi tray atau layanan Windows |

Situs web merespons di port 80, atau 8088 jika port 80 sedang dipakai. WebAdmin ada di `https://localhost:7080/` dengan user `admin` dan kata sandi yang ditampilkan di akhir setup. Menjalankan setup lagi pada instalasi yang sudah ada akan meng-upgrade-nya dan mempertahankan konfigurasi.

<img src="../webadmin.png" width="880" alt="Konsol OpenLiteSpeed WebAdmin yang berjalan di Windows">

Instalasi tanpa interaksi:

```bat
OpenLiteSpeed-1.9.4-win64-setup.exe --silent --mode standalone --download-php 8.4 --edition full
```

| Opsi | Keterangan |
|---|---|
| `--mode standalone\|laragon\|xampp` | Jenis setup, dideteksi otomatis jika tidak diisi |
| `--edition server\|full` | Edisi; jika tidak diisi, `server` untuk instalasi baru dan edisi saat ini untuk upgrade. `--service` memerlukan `server` |
| `--php <folder>` / `--download-php 8.2\|8.3\|8.4\|8.5` | PHP yang dipakai |
| `--dir <folder>` / `--www <folder>` | Folder instalasi / folder proyek |
| `--port <n>` / `--admin-port <n>` | Port situs web dan WebAdmin |
| `--service` / `--no-tray` | Menginstal layanan Windows / tidak menjalankan aplikasi tray |
| `--log <file>` | File log, default `%TEMP%\OpenLiteSpeed-setup.log`, berisi kata sandi admin |

File yang sama tersedia dalam bentuk zip dengan `install.ps1` untuk deployment berbasis skrip. Seperti setup, skrip ini membatasi folder instalasi hanya untuk akun Anda, administrator, dan sistem jika folder tersebut masih mewarisi izin dari folder di atasnya:

```powershell
powershell -ExecutionPolicy Bypass -File install.ps1 -Mode standalone -DownloadPhp 8.4 -Edition full
```

Untuk uninstall, gunakan **Pengaturan > Aplikasi** atau jalankan `uninstall.exe --uninstall --silent` di folder instalasi. Folder proyek dan folder `data` yang berisi database tetap ada.

## Dasbor

Pada edisi full stack, dasbor dibuka dari menu Start atau dengan klik dua kali pada ikon tray. Dasbor ini adalah jendela desktop, bukan halaman web, jadi tidak mendengarkan port apa pun. Halamannya:

| Halaman | Yang bisa Anda lakukan |
|---|---|
| Overview | Menjalankan dan menghentikan web server, MariaDB, dan Cloudflare Tunnel, melihat request per detik dan koneksi yang terbuka |
| Sites | Membuat dan menghapus situs, memilih versi PHP atau Node.js untuk setiap situs, membuat situs online, membuka foldernya, terminal, atau VS Code di dalamnya, menjalankan folder sebagai aplikasi Node.js atau Python, menjalankan tugas latar belakang |
| PHP | Menginstal versi PHP, memilih versi default, menyalakan dan mematikan ekstensi dan Xdebug, mengubah pengaturan `php.ini`, menginstal Composer |
| Node.js | Menginstal versi Node.js, memilih versi yang menjalankan aplikasi, menjalankan ulang aplikasi dan membaca log-nya |
| Databases | Membuat database dan user-nya, mengekspor dan mengimpor file `.sql`, menghapus database, membuka phpMyAdmin |
| Mail | Menyalakan dan mematikan penangkap email, membaca email yang tertangkap beserta bagian HTML dan lampirannya |
| Logs | Membaca log server, MariaDB, aplikasi, dan tunnel, memfilter baris, mengikuti baris baru |
| Settings | Mulai bersama Windows, folder proyek, HTTPS tepercaya, port HTTP dan WebAdmin, serta kata sandi WebAdmin |

**Terminal** di kiri bawah membuka Command Prompt dengan PHP default, Composer, Node.js, dan klien MariaDB di `PATH`.

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../panel-sites-dark.png">
  <img src="../panel-sites-light.png" width="880" alt="Halaman Sites di dasbor dengan versi PHP untuk setiap situs, aplikasi Node.js dan Python, situs proxy, dan sakelar Online">
</picture>

Dasbor memerlukan Microsoft Edge WebView2 Runtime, yang sudah tersedia di Windows 11 dan Windows 10 versi terbaru.

## Situs lokal

Setiap folder di folder proyek adalah situs di `http://folder.test/`. **New site** di dasbor atau tray membuat foldernya. Dasbor menyediakan jenis situs berikut:

| Jenis | Yang Anda dapatkan |
|---|---|
| Situs PHP | Folder berisi `index.php` |
| WordPress | WordPress terbaru. Jika MariaDB terinstal, database dan user untuk situs tersebut serta `wp-config.php` yang sudah terisi, sehingga setup WordPress hanya menanyakan judul situs dan akun admin |
| Laravel | Proyek Laravel baru dari `composer create-project`, disajikan dari folder `public` |
| Aplikasi Node.js | `server.js` dan `package.json` sederhana, dijalankan oleh server dengan Node.js default |
| Aplikasi Python | `wsgi.py` WSGI, dijalankan oleh server dengan Python yang ditemukan di komputer |

Installer WordPress hanya bisa dibuka dari komputer ini, sehingga orang lain di jaringan tidak bisa menyelesaikannya lebih dulu. Folder yang berisi `package.json` menjadi aplikasi Node.js dengan **More > Run as a Node.js app**, dan **Run as a PHP site** mengembalikannya. **Remove site** memindahkan folder ke Recycle Bin dan mempertahankan database.

**New proxy site** memberi nama sendiri untuk aplikasi yang mendengarkan port lokal, misalnya `app.test` untuk server pengembangan Vite di port 5173. WebSocket diteruskan. Situs proxy hanya merespons komputer ini, sama seperti aplikasi di belakangnya.

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../panel-site-dark.png">
  <img src="../panel-site-light.png" width="420" alt="Dialog New site dengan nama situs dan jenis situs: PHP, WordPress, Laravel, aplikasi Node.js, dan aplikasi Python">
</picture>
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../panel-proxy-dark.png">
  <img src="../panel-proxy-light.png" width="420" alt="Dialog New proxy site dengan nama situs dan port aplikasi">
</picture>

## Membuat situs online

Nyalakan **Online** di samping situs pada dasbor, atau jalankan `openlitespeed --tunnel start <site>`. Cloudflare memberi situs alamat acak `https://*.trycloudflare.com` yang dapat dibuka siapa saja sampai Anda mematikannya. Cara ini bekerja di balik NAT, router, atau IP lokal karena koneksi keluar dari komputer Anda, dan tidak ada yang dibuka di firewall. Pengunjung tetap tercatat dengan alamat IP mereka sendiri di log dan di PHP. Alamat publik ditampilkan di bawah alamat lokal, dengan **Copy** di sebelahnya.

Untuk domain Anda sendiri, buat tunnel di Cloudflare Zero Trust, arahkan public hostname-nya ke `http://127.0.0.1` di port situs web dengan nama situs sebagai header HTTP Host, lalu tempel tunnel token di dasbor pada **Use your own domain**. Token disimpan terenkripsi untuk akun Windows Anda.

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../panel-share-dark.png">
  <img src="../panel-share-light.png" width="420" alt="Dialog untuk membagikan situs lokal secara online melalui Cloudflare">
</picture>
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../panel-token-dark.png">
  <img src="../panel-token-light.png" width="420" alt="Dialog untuk menghubungkan domain Anda sendiri dengan tunnel token Cloudflare">
</picture>

Hanya situs dari folder proyek dan situs proxy yang dapat dibuat online. Membagikan situs proxy menghubungkan tunnel langsung ke aplikasinya; dengan domain sendiri, arahkan hostname tersebut ke port aplikasi juga. phpMyAdmin dan WebAdmin hanya merespons komputer ini, dan tunnel tidak akan berjalan kecuali server dapat membedakan pengunjungnya dari komputer ini.

## Database

MariaDB hanya mendengarkan di `127.0.0.1` dan berjalan dengan akun Anda, tidak pernah sebagai administrator. Setup mengunduhnya, dan aplikasi tray membuat database saat pertama kali dijalankan. Kata sandi root dibuat saat itu dan disimpan di `data\client.cnf`, yang hanya dapat dibaca oleh akun Anda dan administrator. `SELECT ... INTO OUTFILE` dan `LOAD DATA` hanya berfungsi di `data\files`. **Copy password** di dasbor atau tray menyalin kata sandi ke clipboard tanpa menambahkannya ke riwayat clipboard. phpMyAdmin ada di `http://phpmyadmin.test/`, hanya untuk komputer ini.

Situs WordPress yang dibuat dari tray atau dasbor mendapat database dan user sendiri, dan `wp-config.php` sudah diisi.

Di halaman **Databases**, **New database** membuat database dengan user bernama sama dan kata sandi yang dibuat otomatis, ditampilkan sekali dengan tombol **Copy**. **Export** menulis file `.sql` dengan `mariadb-dump`, **Import** memuat file `.sql` ke database, dan **Remove** meminta Anda mengetik namanya terlebih dahulu dan dapat menghapus user-nya juga.

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../panel-db-dark.png">
  <img src="../panel-db-light.png" width="880" alt="Halaman Databases di dasbor dengan MariaDB, kata sandi root, serta ekspor, impor, dan hapus untuk setiap database">
</picture>

## HTTPS tepercaya

**Trusted HTTPS** di dasbor atau tray, atau `openlitespeed --dev-https on`, membuat otoritas sertifikat di komputer Anda, meminta Windows memercayainya untuk akun Anda, dan menyajikan setiap situs lokal di `https://name.test/`. Otoritas ini dibatasi oleh name constraints hanya untuk nama `.test`, `.localhost`, dan `.internal` serta `127.0.0.1` dan `::1`, sehingga tidak dapat menandatangani sertifikat untuk situs lain. Mematikannya akan menghapusnya dari trust store Windows.

## PHP

`lsphp.exe` adalah PHP LiteSpeed SAPI yang di-build untuk Windows, untuk PHP 8.2 hingga 8.5 dalam varian thread safe dan non thread safe. Ia berjalan di atas build PHP resmi dari [windows.php.net](https://windows.php.net), sehingga ekstensi dan pengaturan `php.ini` yang sudah ada tetap berfungsi. Jika suatu versi PHP tidak dapat memuatnya, OpenLiteSpeed beralih ke `php-cgi.exe` melalui FastCGI.

<img src="../phpinfo.png" width="640" alt="phpinfo PHP 8.4 di Windows dengan Server API LiteSpeed">

Ganti versi PHP dari dasbor, menu tray, atau dengan `openlitespeed --set-php <folder>`.

**Install PHP** di halaman PHP menampilkan versi dari windows.php.net, dari 7.4 hingga 8.5, thread safe dan non thread safe. Unduhan diperiksa dengan SHA-256 yang dipublikasikan windows.php.net, diekstrak ke `php\<version>`, dan mendapat `php.ini` dengan ekstensi umum yang sudah aktif. Versi 8.2 ke atas berjalan melalui LSAPI, versi yang lebih lama melalui FastCGI.

Setiap situs dapat memakai versi PHP sendiri: pilih di kolom PHP pada halaman Sites, atau jalankan `openlitespeed --site php <site> <folder>`. Situs tanpa pilihan mengikuti versi default. Untuk setiap versi, menu **More** berisi **Extensions**, sakelar untuk setiap ekstensi PHP tersebut, dan **Settings** untuk batas memori, ukuran upload dan POST, waktu eksekusi, variabel input, tampilan error, dan zona waktu. **Edit php.ini** membuka file di Notepad. Perubahan akan me-reload web server.

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../panel-php-dark.png">
  <img src="../panel-php-light.png" width="880" alt="Halaman PHP di dasbor dengan tiga versi PHP, versi default, situs yang memakai setiap versi, dan Composer">
</picture>

**Install** di bawah Composer mengunduh `composer.phar` dari getcomposer.org dan memeriksanya dengan SHA-256. Terminal dasbor memiliki `composer` di `PATH`, dan situs Laravel dibuat dengannya.

## Xdebug

Sakelar di kolom Xdebug pada halaman PHP, atau `openlitespeed --php xdebug <folder> on`, menyalakan Xdebug untuk versi PHP tersebut. Xdebug yang sudah dimuat dari folder `ext` akan dipakai. Jika tidak ada, build Windows resmi untuk versi PHP tersebut diunduh dari xdebug.org dan diperiksa dengan SHA-256 yang disimpan di OpenLiteSpeed: Xdebug 3.5.3 untuk PHP 8.2 hingga 8.5, thread safe dan non thread safe.

Xdebug dimulai di setiap request dan terhubung ke debugger di `127.0.0.1:9003`. Tanpa debugger, request langsung dilanjutkan dan tidak ada yang ditulis ke log. JIT opcache dimatikan selama Xdebug menyala dan aktif kembali saat Xdebug dimatikan. Pengaturannya disimpan di akhir `php.ini`, dan web server di-reload agar pengaturan berlaku.

Xdebug tidak dapat dinyalakan selama OpenLiteSpeed terinstal sebagai layanan Windows. Program apa pun di komputer yang mendengarkan port 9003 akan menerima sesi debug dan dapat menjalankan kode PHP sebagai akun layanan, jadi matikan Xdebug sebelum Anda menginstal layanan.

**VS Code:** **More > Open in VS Code** di halaman Sites membuka folder situs di VS Code jika sudah terinstal. Mulai sesi debug PHP yang mendengarkan port 9003, misalnya dengan ekstensi PHP Debug, lalu buka situs di browser.

## Penangkap email

Sakelar di halaman Mail, atau `openlitespeed --mail on`, menangkap setiap email yang dikirim situs lokal, sehingga tidak ada yang keluar dari komputer. PHP `mail()` diteruskan ke OpenLiteSpeed melalui `sendmail_path` di `php.ini` dari versi PHP yang dijalankan OpenLiteSpeed, dan aplikasi yang mengirim melalui SMTP memakai host `127.0.0.1`, port 1025, tanpa enkripsi, dan nama user serta kata sandi apa saja. Situs Laravel baru mendapat pengaturan ini di `.env`. Koneksi yang diawali seperti request web akan ditutup, sehingga halaman web di browser tidak dapat mengirim email ke penangkap email.

Email disimpan di `data\mail`, yang hanya dapat dibaca oleh akun Anda, hingga 1000 pesan terbaru dan 512 MB. Halaman Mail menampilkan setiap pesan dengan bagian teks dan HTML-nya dan memungkinkan Anda menyimpan lampiran. Bagian HTML ditampilkan tanpa skrip dan tidak memuat apa pun dari internet.

## Tugas latar belakang

**Background jobs** di menu **More** sebuah situs, atau `openlitespeed --job add <site> <name> <command>`, menjalankan perintah yang berjalan lama seperti `php artisan queue:work`, `php artisan schedule:work`, atau `npm run dev` di folder situs, dengan versi PHP dan Node.js situs tersebut di urutan pertama `PATH`. Tugas dimulai dan berhenti bersama web server. Tugas yang berhenti dijalankan lagi setelah 1, 2, 4, hingga 60 detik, dan setelah 10 kali dijalankan ulang dalam 10 menit, tugas tersebut tetap berhenti. Menghentikan tugas akan mengakhiri setiap proses yang dijalankannya. Output ditulis ke `logs\jobs\<site>.<name>.log`, yang ditampilkan di halaman Logs. Dengan layanan Windows, akun layanan menjalankan tugas dan penangkap email, sama seperti menjalankan PHP.

## Node.js

**Install Node.js** di halaman Node.js menampilkan versi Current dan LTS dari nodejs.org. Unduhan diperiksa dengan `SHASUMS256.txt` dan diekstrak ke `node\<version>`. Halaman ini juga menampilkan Node.js yang ditemukan di `Program Files`, Laragon, nvm, Volta, dan di `PATH`. **Make default** memilih versi yang menjalankan aplikasi Node.js, dan aplikasinya ditampilkan di bawahnya dengan **Restart** dan **Log**.

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../panel-node-dark.png">
  <img src="../panel-node-light.png" width="880" alt="Halaman Node.js di dasbor dengan versi Node.js yang terinstal dan aplikasi yang sedang berjalan">
</picture>

Aplikasi Node.js tidak perlu diubah: `app.listen(3000)` dipindahkan ke alamat yang diberikan server. Lihat [docs/APPS.md](../APPS.md).

Setiap situs dapat memakai Node.js sendiri: pilih di kolom Version pada halaman Sites, atau jalankan `openlitespeed --site node <site> <node.exe>`. Tanpa pilihan, file `.nvmrc` atau `.node-version` yang menyebutkan versi mayor seperti `20` akan memilih Node.js 20 terbaru yang terinstal, dan jika tidak ada, situs dijalankan dengan versi default. Tugas latar belakang dan terminal situs memakai versi yang sama.

## Pindah dari Apache

OpenLiteSpeed membaca konfigurasi Apache yang sudah ada dan membuat satu situs untuk setiap `<VirtualHost>`, lengkap dengan domain, document root, sertifikat SSL, redirect, aturan rewrite, alias, proxy, aturan akses, dan proteksi kata sandinya. Aturan di file `.htaccess` tetap berfungsi karena OpenLiteSpeed menerapkannya sendiri.

```bat
bin\openlitespeed.exe --import-apache --dry-run
bin\openlitespeed.exe --import-apache C:\Apache24\conf\httpd.conf
bin\openlitespeed.exe -s reload
```

Tanpa path, konfigurasi Apache dari Laragon, XAMPP, WampServer, atau `C:\Apache24` ditemukan secara otomatis. `--dry-run` mencetak laporan tanpa mengubah apa pun. Mengimpor lagi akan menggantikan impor sebelumnya, dan `--import-apache --remove` menghapusnya. Aturan yang tidak dapat dikonversi dicantumkan dalam laporan. Aturan akses dikonversi persis sama atau dibuat lebih ketat, tidak pernah lebih longgar; satu-satunya pengecualian, aturan yang memblokir klien tertentu berdasarkan nama host atau variabel lingkungan, dilaporkan sebagai peringatan. Detailnya ada di [docs/APACHE.md](../APACHE.md).

## Pindah dari nginx

`--import-nginx` melakukan hal yang sama untuk nginx: setiap blok `server` menjadi situs lengkap dengan domain, listener, sertifikat SSL, location, `try_files`, rewrite, redirect, alias, proxy, aturan akses, dan proteksi kata sandinya, dan PHP di belakang `fastcgi_pass` berjalan melalui PHP milik OpenLiteSpeed.

```bat
bin\openlitespeed.exe --import-nginx --dry-run
bin\openlitespeed.exe --import-nginx C:\nginx\conf\nginx.conf
```

Detailnya ada di [docs/NGINX.md](../NGINX.md).

## Banyak core CPU

OpenLiteSpeed menjalankan satu worker HTTP per core CPU, maksimal 8 secara default. Atur **Number of Workers** di **Server Configuration > General** pada WebAdmin, atau `httpdWorkers` di `conf\httpd_config.conf`, ke nilai berapa pun dari 1 hingga 32. Setiap worker memiliki pool PHP sendiri. Lihat [docs/ARCHITECTURE.md](../ARCHITECTURE.md) tentang cara worker berbagi socket listening di Windows.

## Command line

Jalankan perintah di folder instalasi sebagai `bin\openlitespeed.exe`.

| Perintah | Keterangan |
|---|---|
| `openlitespeed -s start\|stop\|restart\|reload\|status` | Mengontrol server, `reload` adalah restart tanpa downtime |
| `openlitespeed -d` | Berjalan di foreground |
| `openlitespeed --install-service` / `--uninstall-service` | Mendaftarkan atau menghapus layanan Windows |
| `openlitespeed --set-php <folder>` | Memakai versi PHP lain |
| `openlitespeed --import-apache [httpd.conf] [--dry-run]` / `--import-apache --remove` | Mengimpor situs Apache / menghapus situs yang diimpor |
| `openlitespeed --import-nginx [nginx.conf] [--dry-run]` / `--import-nginx --remove` | Mengimpor situs nginx / menghapus situs yang diimpor |
| `openlitespeed --acme-install [--email <address>]` | Mengaktifkan sertifikat Let's Encrypt otomatis |
| `openlitespeed --modsec-install [--detection-only]` / `--modsec-install --remove` | Menyalakan atau mematikan firewall ModSecurity dengan OWASP Core Rule Set |
| `openlitespeed --dev-sites <folder> [tld]` | Membuat situs untuk setiap subfolder dari sebuah folder |
| `openlitespeed --new-site <name> [--wordpress\|--laravel\|--node\|--python]` | Membuat situs di folder proyek: situs PHP, WordPress dengan database-nya, proyek Laravel, aplikasi Node.js, atau aplikasi Python |
| `openlitespeed --site php <site> <folder>\|default` | Memakai versi PHP lain untuk satu situs |
| `openlitespeed --site app <site> node\|python\|off` | Menjalankan folder situs sebagai aplikasi Node.js atau Python, atau kembali sebagai situs PHP |
| `openlitespeed --site node <site> <node.exe>\|default` | Memakai versi Node.js lain untuk satu situs |
| `openlitespeed --php list` | Versi PHP di komputer ini dalam JSON, beserta situs dan status Xdebug masing-masing |
| `openlitespeed --php available` / `install <8.x> [ts]` / `remove <folder>` | Menampilkan dan menginstal versi PHP dari windows.php.net, menghapus versi yang sudah diunduh |
| `openlitespeed --php ext <folder>` / `ext-set <folder> <name> on\|off` | Menampilkan ekstensi sebuah versi PHP, menyalakan atau mematikan salah satunya |
| `openlitespeed --php ini <folder>` / `ini-set <folder> <key> <value>` | Membaca atau mengubah pengaturan `php.ini` yang umum |
| `openlitespeed --php xdebug <folder> on\|off\|status` | Menyalakan atau mematikan Xdebug untuk sebuah versi PHP, atau menampilkan apakah sedang aktif |
| `openlitespeed --node list\|available` / `install <major>` / `default <node.exe>` / `remove <node.exe>` | Mengelola versi Node.js untuk aplikasi |
| `openlitespeed --composer install` | Mengunduh Composer ke `tools\composer` |
| `openlitespeed --set-port http\|admin <port>` | Mengubah port situs web atau WebAdmin |
| `openlitespeed --proxy add <name> <port>` / `remove <name>` / `list` | Menambah atau menghapus situs proxy untuk aplikasi di port lokal |
| `openlitespeed --dev-https on [port]\|off\|status` | HTTPS tepercaya untuk situs lokal |
| `openlitespeed --dev-hosts` / `--dev-hosts-clear` | Menambah atau menghapus nama situs di file hosts |
| `openlitespeed --db install\|fetch\|start\|stop\|status` | Menginstal, hanya mengunduh, menjalankan, atau menghentikan MariaDB dan phpMyAdmin |
| `openlitespeed --db list` / `create <name> [--no-user]` / `drop <name> [--user]` | Menampilkan, membuat, atau menghapus database |
| `openlitespeed --db export <name> <file.sql>` / `import <name> <file.sql>` | Mengekspor database ke file atau mengimpor file ke database |
| `openlitespeed --tunnel start <site>` / `stop <site>` / `stop --all` / `status` | Membuat situs online dengan Cloudflare Tunnel atau membuatnya offline |
| `openlitespeed --tunnel token` / `run` / `run-stop` / `token clear` | Menyimpan tunnel token dari standard input, menghubungkan atau memutus domain Anda sendiri, melupakan token |
| `openlitespeed --mail on\|off\|status` | Menyalakan atau mematikan penangkap email, atau menampilkan apakah sedang berjalan |
| `openlitespeed --mail list` / `remove <id>` / `save <id> <n> <file>` / `clear` | Menampilkan email yang tertangkap dalam JSON, menghapus satu pesan, menyimpan lampiran `n` (mulai dari 0) dari sebuah pesan ke file, atau menghapus semua pesan |
| `openlitespeed --mail sendmail [-t] [-i] [-f <from>] [<recipient>...]` | Menyimpan pesan dari standard input, seperti sendmail |
| `openlitespeed --job list` / `add <site> <name> <command>` / `remove <site> <name>` | Menampilkan, menambah, atau menghapus tugas latar belakang situs |
| `openlitespeed --job start\|stop\|restart <site> <name>` / `autostart <site> <name> on\|off` | Mengontrol tugas, memilih apakah tugas dimulai bersama web server |
| `openlitespeed --status` | Server, PHP, situs, database, dan tunnel dalam JSON |
| `openlitespeed --admin-password admin\conf\htpasswd <password>` | Mengatur kata sandi WebAdmin |

Aplikasi tray menjalankan, menghentikan, dan me-reload server, membuka situs, WebAdmin, dan log, membuat situs dan situs proxy, menyalakan dan mematikan HTTPS tepercaya, mengganti versi PHP, dan mengimpor situs Apache dan nginx. Pada edisi full stack, aplikasi ini juga membuka dasbor dan mengontrol MariaDB. Sekali sehari aplikasi ini memeriksa rilis baru di GitHub; **Update** mengunduhnya, memeriksa tanda tangan dan checksum, lalu menjalankan setup yang baru. `check=0` di bawah `[update]` dalam `conf\olstray.ini` mematikan pemeriksaan ini.

## Build dari source

Lihat [docs/BUILD.md](../BUILD.md). Build memakai Visual Studio 2022, CMake, dan Ninja, serta mengambil semua dependensi pada commit yang dipatok.

## Keterbatasan

- Tidak ada pergantian user per virtual host (suEXEC). PHP berjalan sebagai user yang menjalankan server, atau sebagai akun layanan.
- Tidak ada `SO_REUSEPORT`. Worker berbagi satu socket listening per alamat.
- cgroups, namespaces, bubblewrap, dan CageFS adalah fitur Linux dan tidak tersedia.
- Aplikasi Ruby melalui konteks App Server tidak didukung.

## Kontribusi

Laporan bug dan pull request sangat diterima, lihat [CONTRIBUTING.md](../../CONTRIBUTING.md). Laporkan masalah keamanan secara pribadi seperti yang dijelaskan di [SECURITY.md](../../SECURITY.md).

## Lisensi

GPL-3.0, lihat [LICENSE](../../LICENSE). OpenLiteSpeed dikembangkan oleh LiteSpeed Technologies dan dilisensikan di bawah GPL-3.0. `lsphp.exe` di-build dari source PHP dan tercakup dalam [PHP License](../../win/lsphp/PHP-LICENSE.txt). Komponen pihak ketiga tercantum di [THIRD-PARTY-NOTICES.txt](../../win/package/THIRD-PARTY-NOTICES.txt).

Proyek ini tidak berafiliasi dengan atau didukung oleh LiteSpeed Technologies maupun PHP Group. LiteSpeed dan OpenLiteSpeed adalah merek dagang LiteSpeed Technologies, Inc.

Dari pembuat yang sama: [OpenLiteSpeed for cPanel and WHM](https://github.com/NeuralFoundry/openlitespeed-cpanel).
