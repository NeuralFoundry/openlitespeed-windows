[English](../../README.md) | [Türkçe](README.tr.md) | [简体中文](README.zh-CN.md) | [Español](README.es.md) | [Português](README.pt-BR.md) | [Русский](README.ru.md) | **日本語** | [Deutsch](README.de.md) | [Français](README.fr.md) | [한국어](README.ko.md) | [Bahasa Indonesia](README.id.md)

# OpenLiteSpeed for Windows

Web サイト: [openlitespeed.dev/windows](https://openlitespeed.dev/windows/)

Windows版 OpenLiteSpeed は、[OpenLiteSpeed](https://openlitespeed.org) Web サーバー 1.9.4 のネイティブ Windows ビルドです。WSL、Docker、仮想マシンなしで Windows 10、Windows 11、Windows Server 上で動作し、PHP を LSAPI または FastCGI で実行します。単体でも、Laragon や XAMPP の中にもインストールできます。フルスタック エディションにはダッシュボード、MariaDB、phpMyAdmin、Cloudflare Tunnel が加わり、ローカル IP の背後にあるパソコン上のサイトでもインターネットに公開できます。ダッシュボードからは PHP と Node.js のバージョンのインストール、サイトごとの PHP バージョンの指定、WordPress、Laravel、Node.js、Python サイトの作成、データベースの管理ができます。GPL-3.0 で提供される無料のオープンソースソフトウェアです。

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../panel-dark.png">
  <img src="../panel-light.png" width="880" alt="Windows版 OpenLiteSpeed のダッシュボード概要。Web サーバー、MariaDB、Cloudflare Tunnel、リアルタイムのリクエスト数、ローカル サイト">
</picture>

## 機能

- Visual C++ でビルドしたネイティブ Win32 プログラム。グラフィカルなインストーラー、トレイ アプリ、Windows サービスを備えています
- 1 つのインストーラーに 2 つのエディション: Web サーバーのみ、またはダッシュボード、MariaDB、phpMyAdmin、Cloudflare Tunnel を含むフルスタック
- ダッシュボード: サービスの起動と停止、サイトの作成、PHP と Node.js のインストール、データベースの管理、ログの閲覧、ポートの変更、サイトの公開を 1 つのウィンドウで行えます
- PHP 7.4 から 8.5 と Node.js の LTS バージョンをダッシュボードからインストールし、公開されている SHA-256 で検証します
- サイトごとの PHP バージョン、スイッチでオンとオフを切り替えられる PHP 拡張機能、よく使う `php.ini` の設定を編集できるフォーム
- 新しいサイトの種類: シンプルな PHP サイト、WordPress、Laravel（Composer 付き）、Node.js アプリ、Python アプリ
- データベース: 専用ユーザー付きのデータベースの作成、`.sql` ファイルのエクスポートとインポート、データベースの削除
- Cloudflare Tunnel でローカル サイトを公開: ポート転送もグローバル IP も不要で、パソコン上のサイトに公開 https アドレスを割り当てます。トンネル トークンを使えば独自ドメインも使えます。
- MariaDB 11.8 と phpMyAdmin 5.2 は 127.0.0.1 のみで待ち受け、root パスワードは自動生成されます。新しい WordPress サイトにはそれぞれ専用のデータベースとユーザーが作成されます。
- すべての CPU コアを活用: 同じポートで最大 32 個の HTTP ワーカー プロセスが動作し、クラッシュ後は自動で再起動し、ダウンタイムなしでリロードできます
- PHP 8.2、8.3、8.4、8.5 を LSAPI（Windows 用の `lsphp.exe` を同梱）または FastCGI（`php-cgi.exe`）で実行
- インストール済みの PHP バージョン、Laragon、XAMPP を検出し、それに合わせて自動で設定します
- `httpd.conf` から Apache のサイトをインポート: バーチャルホスト、SSL 証明書、リダイレクト、エイリアス、プロキシ、CGI、アクセス ルール、パスワード保護に加え、`.htaccess` ファイル内のルールにも対応します
- CGI プログラム: `.exe`、`.bat`、`.cmd` ファイルと、Perl や Python など `#!` 行を持つスクリプト。それぞれが制限付きトークンを持つ専用のジョブ オブジェクト内で実行されます
- サーバーが起動と再起動を管理する Node.js と Python（WSGI）アプリケーション。Node.js アプリは [openlitespeed-node](https://github.com/NeuralFoundry/openlitespeed-node) により変更なしで動作し、WebSocket にも対応します。[docs/APPS.md](../APPS.md) を参照してください
- ローカル開発サイト: プロジェクト フォルダー内の各フォルダーが `http://folder.test/` になります。トレイまたはダッシュボードから新しいサイトを作成でき、WordPress も選べます
- ローカル サイト向けの信頼された HTTPS: パソコン上で作成した認証局で、`.test`、`.localhost`、`.internal` の名前とそのパソコン自身のアドレスにしか署名できません
- ローカル ポートで動くアプリ向けのプロキシ サイト。たとえば `127.0.0.1:3000` 上のアプリに `api.test` を割り当てられます
- 署名付きアップデート: トレイ アプリが新しいリリースをダウンロードし、署名を検証して、確認後にインストールします
- HTTP/2、HTTP/3（QUIC）、TLS 1.3、Brotli、LSCache、WebAdmin コンソール
- Let's Encrypt の無料 HTTPS 証明書を、内蔵の ACME クライアントが自動で取得、更新します。[docs/ACME.md](../ACME.md) を参照してください
- 内蔵モジュール: LSCache、アップロード進捗、リクエスト ボディの解析、LuaJIT による Lua スクリプト。[docs/MODULES.md](../MODULES.md) を参照してください
- `.htaccess` ファイルは Apache と同じように動作します: リライト ルール、アクセス ルール（`Require`、`Order`、`Allow`、`Deny`）、パスワード、`<Files>` セクション、`Options Indexes`、`php_flag engine off`。ファイルを変更すると 1 秒以内に反映されます
- Web アプリケーション ファイアウォール: OWASP Core Rule Set を使う ModSecurity 3 を 1 つのコマンドで有効にできます。[docs/PROTECTION.md](../PROTECTION.md#web-application-firewall) を参照してください
- ボット対策: reCAPTCHA または hCaptcha のチャレンジ、クライアント IP ごとのリクエスト数、帯域、接続数の制限、MaxMind GeoIP2 または IP2Location データベースによる訪問者の国情報（PHP とリライト ルールで利用可能）。[docs/PROTECTION.md](../PROTECTION.md) を参照してください
- WordPress 用の LiteSpeed Cache はそのまま使えます: プラグインをインストールして有効にするだけで、サーバーの設定は不要です。コンテンツが変更されるとページ キャッシュは自動で削除されます。
- Windows 向けの強化: 大文字と小文字、短いファイル名、データ ストリーム、デバイス名を悪用する手口に対してファイル名を検査し、PHP は制限付きトークンで、サービスは仮想アカウントで実行します

## パフォーマンス: Windows版 OpenLiteSpeed と nginx、Apache の比較

このリポジトリのネイティブ Windows ビルドを、同じ Windows 11 マシン上で nginx と Apache と比較しました。PHP は 8.4 で、どのサーバーにも同じ 12 個の CPU コアを割り当てています。値は 1 秒あたりのリクエスト数で、大きいほど高速です。すべての計測はエラーなしで完了しました。

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../bench-windows-overview-dark.svg">
  <img src="../bench-windows-overview-light.svg" width="880" alt="Windows版 OpenLiteSpeed と nginx、Apache の比較。静的ファイル、PHP アプリケーション、WordPress の 1 秒あたりのリクエスト数">
</picture>

| サーバー | 静的ファイル | PHP アプリケーション | WordPress（キャッシュなし） | HTTPS 経由の PHP | 512 接続での PHP |
|---|--:|--:|--:|--:|--:|
| OpenLiteSpeed + LSAPI | 175,203 | 9,548 | 117 | 8,687 | 9,434 |
| OpenLiteSpeed + FastCGI |  | 9,538 | 119 | 8,961 | 9,587 |
| nginx + PHP-CGI | 60,306 | 2,949 | 122 | 472 | 462 |
| Apache + mod_php | 17,889 | 7,532 | 105 | 4,473 | 3,078 |
| Apache + PHP-CGI |  | 5,743 | 118 | 4,356 | 5,396 |

静的ファイルは PHP を使わないため、サーバーごとに 1 回だけ計測しました。静的ファイル、PHP アプリケーション、WordPress: 128 の keep-alive 接続（WordPress は 64）で各 20 秒。HTTPS: 10 秒、128 接続。最後の列は、512 接続で 60 秒間続けた負荷テストの平均値です:

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../bench-windows-stress-app-dark.svg">
  <img src="../bench-windows-stress-app-light.svg" width="880" alt="Windows版 OpenLiteSpeed の負荷テスト。512 接続で 60 秒間、10 秒ごとに計測した 1 秒あたりのリクエスト数">
</picture>

キャッシュなしの WordPress は、処理の大半を PHP と MySQL が担うため、どのサーバーでもほぼ同じ速度になります。ページ キャッシュを使うと結果は変わります。同じサイトのトップページ、64 接続:

| サーバーとキャッシュ | リクエスト/秒 | p99 |
|---|--:|--:|
| OpenLiteSpeed + LiteSpeed Cache プラグイン | 21,180 | 6.1 ms |
| Apache + mod_cache_disk | 18,362 | 6.1 ms |
| nginx + fastcgi_cache | 7,745 | 10.8 ms |

レイテンシ、メモリ、計測環境の詳細、生データは [docs/BENCHMARKS.md](../BENCHMARKS.md) にあります。

## 動作要件

- Windows 10、Windows 11、または Windows Server 2019 以降、x64
- Windows 用 PHP 8.2 以降、x64。インストーラーがダウンロードすることもできます。

## インストール

[Releases](https://github.com/NeuralFoundry/openlitespeed-windows/releases/latest) から `OpenLiteSpeed-1.9.4-win64-setup.exe` をダウンロードして実行します。セットアップは検出した PHP バージョンを表示し、セットアップの種類を提案して、エディションを尋ねます。

<img src="../setup.png" width="576" alt="Windows版 OpenLiteSpeed のセットアップ画面。エディションの選択と PHP バージョン">

| エディション | 含まれるもの |
|---|---|
| フルスタック | OpenLiteSpeed、PHP、トレイ アプリ、ダッシュボード、MariaDB、phpMyAdmin、Cloudflare Tunnel。セットアップは約 170 MB をダウンロードし、すべてのファイルを固定された SHA-256 で検証します。 |
| Web サーバーのみ | OpenLiteSpeed、PHP、トレイ アプリ。新しいサイトの作成、信頼された HTTPS、PHP の切り替え、インポート、アップデートに対応します |

セットアップをもう一度実行するとエディションを切り替えられます。Web サーバーのみに切り替えても、データベースは残ります。フルスタック エディションはトレイ アプリと一緒に動作するため、Windows サービスのオプションは Web サーバーのみのエディション専用です。

セットアップはインストール先フォルダーへのアクセスを、あなたのアカウント、管理者、システムだけに制限します。そのため、パソコンの他のユーザーはプログラムや設定を変更できません。Laragon と XAMPP のフォルダーは変更しませんが、これらの PHP フォルダーは他のユーザーも変更できることがよくあります。Windows サービスを使う場合、それによって他のユーザーがサービス アカウントとしてコードを実行できてしまうため、サービスのインストール時にはそのような PHP フォルダーについて警告が表示されます。

| セットアップの種類 | インストール先フォルダー | プロジェクト フォルダー | 起動方法 |
|---|---|---|---|
| スタンドアロン サーバー | `C:\OpenLiteSpeed` | `C:\OpenLiteSpeed\www` | トレイ アプリまたは Windows サービス |
| Laragon 内 | `C:\laragon\bin\openlitespeed` | Laragon のドキュメント ルート | Laragon **Start All** |
| XAMPP 内 | `C:\xampp\openlitespeed` | `C:\xampp\htdocs` | トレイ アプリまたは Windows サービス |

Web サイトはポート 80 で応答し、ポート 80 が使用中の場合は 8088 を使います。WebAdmin は `https://localhost:7080/` にあり、ユーザー `admin` と、セットアップの最後に表示されるパスワードでログインします。既存のインストールに対してセットアップを再実行すると、設定を保ったままアップグレードします。

<img src="../webadmin.png" width="880" alt="Windows で動作する OpenLiteSpeed WebAdmin コンソール">

無人インストール:

```bat
OpenLiteSpeed-1.9.4-win64-setup.exe --silent --mode standalone --download-php 8.4 --edition full
```

| オプション | 説明 |
|---|---|
| `--mode standalone\|laragon\|xampp` | セットアップの種類。省略すると自動検出します |
| `--edition server\|full` | エディション。省略すると、新規インストールでは `server`、アップグレードでは現在のエディションになります。`--service` には `server` が必要です |
| `--php <folder>` / `--download-php 8.2\|8.3\|8.4\|8.5` | 使用する PHP |
| `--dir <folder>` / `--www <folder>` | インストール先フォルダー / プロジェクト フォルダー |
| `--port <n>` / `--admin-port <n>` | Web サイトと WebAdmin のポート |
| `--service` / `--no-tray` | Windows サービスをインストール / トレイ アプリを起動しない |
| `--log <file>` | ログ ファイル。既定は `%TEMP%\OpenLiteSpeed-setup.log` で、管理者パスワードが含まれます |

スクリプトによる展開向けに、同じファイルを `install.ps1` 付きの zip でも提供しています。セットアップと同じく、インストール先フォルダーがまだ上位フォルダーからアクセス許可を継承している場合は、アクセスをあなたのアカウント、管理者、システムだけに制限します:

```powershell
powershell -ExecutionPolicy Bypass -File install.ps1 -Mode standalone -DownloadPhp 8.4 -Edition full
```

アンインストールするには、**設定 > アプリ** を使うか、インストール先フォルダーで `uninstall.exe --uninstall --silent` を実行します。プロジェクト フォルダーと、データベースを含む `data` フォルダーは残ります。

## ダッシュボード

フルスタック エディションでは、スタート メニューから、またはトレイ アイコンをダブルクリックしてダッシュボードを開きます。Web ページではなくデスクトップ ウィンドウなので、ポートを待ち受けることはありません。ページは次のとおりです:

| ページ | できること |
|---|---|
| Overview | Web サーバー、MariaDB、Cloudflare Tunnel の起動と停止、1 秒あたりのリクエスト数と接続数の確認 |
| Sites | サイトの作成と削除、サイトごとの PHP または Node.js バージョンの選択、サイトの公開、フォルダーやその中でのターミナル、VS Code を開く、フォルダーを Node.js または Python アプリとして実行、バックグラウンド ジョブの実行 |
| PHP | PHP バージョンのインストール、既定バージョンの選択、拡張機能と Xdebug のオンとオフ、`php.ini` の設定変更、Composer のインストール |
| Node.js | Node.js バージョンのインストール、アプリを実行するバージョンの選択、アプリの再起動とログの閲覧 |
| Databases | データベースとそのユーザーの作成、`.sql` ファイルのエクスポートとインポート、データベースの削除、phpMyAdmin を開く |
| Mail | メール キャッチャーのオンとオフ、受信したメールを HTML パートと添付ファイルとともに閲覧 |
| Logs | サーバー、MariaDB、アプリ、トンネルのログの閲覧、行の絞り込み、新しい行の追跡 |
| Settings | Windows と同時に起動、プロジェクト フォルダー、信頼された HTTPS、HTTP と WebAdmin のポート、WebAdmin のパスワード |

左下の **Terminal** を押すと、既定の PHP、Composer、Node.js、MariaDB クライアントが `PATH` に入ったコマンド プロンプトが開きます。

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../panel-sites-dark.png">
  <img src="../panel-sites-light.png" width="880" alt="ダッシュボードの Sites ページ。サイトごとの PHP バージョン、Node.js と Python のアプリ、プロキシ サイト、Online スイッチ">
</picture>

ダッシュボードには Microsoft Edge WebView2 Runtime が必要です。Windows 11 と最新の Windows 10 には標準で含まれています。

## ローカル サイト

プロジェクト フォルダー内の各フォルダーは、`http://folder.test/` のサイトになります。ダッシュボードまたはトレイの **New site** でフォルダーを作成します。ダッシュボードで選べるサイトの種類は次のとおりです:

| 種類 | 作成されるもの |
|---|---|
| PHP サイト | `index.php` を含むフォルダー |
| WordPress | 最新の WordPress。MariaDB がインストールされている場合は、サイト用のデータベースとユーザー、入力済みの `wp-config.php` も用意されるので、WordPress のセットアップではサイトのタイトルと管理者アカウントだけを入力します |
| Laravel | `composer create-project` で作成した新しい Laravel プロジェクト。`public` フォルダーから配信されます |
| Node.js アプリ | 小さな `server.js` と `package.json`。既定の Node.js でサーバーが起動します |
| Python アプリ | WSGI の `wsgi.py`。パソコン上で見つかった Python でサーバーが起動します |

WordPress のインストーラーはこのパソコンからしか開けないため、ネットワーク上の他の人が先にインストールを完了させることはできません。`package.json` を含むフォルダーは **More > Run as a Node.js app** で Node.js アプリになり、**Run as a PHP site** で元に戻ります。**Remove site** はフォルダーをごみ箱に移動し、データベースは残します。

**New proxy site** は、ローカル ポートで待ち受けるアプリに独自の名前を付けます。たとえばポート 5173 の Vite 開発サーバーに `app.test` を割り当てられます。WebSocket もそのまま転送されます。プロキシ サイトは、その背後のアプリと同じく、このパソコンからのアクセスにのみ応答します。

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../panel-site-dark.png">
  <img src="../panel-site-light.png" width="420" alt="New site ダイアログ。サイト名と、PHP、WordPress、Laravel、Node.js アプリ、Python アプリのサイトの種類">
</picture>
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../panel-proxy-dark.png">
  <img src="../panel-proxy-light.png" width="420" alt="New proxy site ダイアログ。サイト名とアプリのポート">
</picture>

## サイトをインターネットに公開する

ダッシュボードでサイトの横にある **Online** をオンにするか、`openlitespeed --tunnel start <site>` を実行します。Cloudflare がサイトにランダムな `https://*.trycloudflare.com` アドレスを割り当て、オフにするまで誰でもアクセスできます。接続はパソコンから外向きに張られるため、NAT やルーター、ローカル IP の背後でも動作し、ファイアウォールで何かを開放する必要はありません。訪問者の IP アドレスは、ログでも PHP でも本来のものが保持されます。公開アドレスはローカル アドレスの下に表示され、横に **Copy** があります。

独自ドメインを使う場合は、Cloudflare Zero Trust でトンネルを作成し、その公開ホスト名を Web サイトのポートの `http://127.0.0.1` に向け、HTTP Host ヘッダーにサイト名を指定してから、ダッシュボードの **Use your own domain** にトンネル トークンを貼り付けます。トークンは Windows アカウント用に暗号化して保存されます。

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../panel-share-dark.png">
  <img src="../panel-share-light.png" width="420" alt="Cloudflare 経由でローカル サイトをインターネットに共有するダイアログ">
</picture>
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../panel-token-dark.png">
  <img src="../panel-token-light.png" width="420" alt="Cloudflare のトンネル トークンで独自ドメインを接続するダイアログ">
</picture>

公開できるのは、プロジェクト フォルダー内のサイトとプロキシ サイトだけです。プロキシ サイトを共有すると、トンネルはそのアプリに直接接続します。独自ドメインを使う場合は、そのホスト名もアプリのポートに向けてください。phpMyAdmin と WebAdmin はこのパソコンからのアクセスにのみ応答し、サーバーが訪問者とこのパソコンを区別できない場合はトンネルが起動しません。

## データベース

MariaDB は `127.0.0.1` のみで待ち受け、管理者としてではなく、常にあなたのアカウントで動作します。セットアップが MariaDB をダウンロードし、トレイ アプリが初回起動時にデータベースを作成します。root パスワードはそのときに生成され、あなたのアカウントと管理者だけが読める `data\client.cnf` に保存されます。`SELECT ... INTO OUTFILE` と `LOAD DATA` は `data\files` 内でのみ動作します。ダッシュボードまたはトレイの **Copy password** は、クリップボードの履歴に残さずにパスワードをクリップボードにコピーします。phpMyAdmin は `http://phpmyadmin.test/` にあり、このパソコンからのみ利用できます。

トレイまたはダッシュボードから作成した WordPress サイトには、専用のデータベースとユーザーが作成され、`wp-config.php` も入力済みになります。

**Databases** ページの **New database** は、同じ名前のユーザーと自動生成されたパスワード付きでデータベースを作成します。パスワードは **Copy** ボタンとともに一度だけ表示されます。**Export** は `mariadb-dump` で `.sql` ファイルを書き出し、**Import** は `.sql` ファイルをデータベースに読み込みます。**Remove** は先に名前の入力を求め、ユーザーも一緒に削除できます。

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../panel-db-dark.png">
  <img src="../panel-db-light.png" width="880" alt="ダッシュボードの Databases ページ。MariaDB、root パスワード、各データベースのエクスポート、インポート、削除">
</picture>

## 信頼された HTTPS

ダッシュボードまたはトレイの **Trusted HTTPS**、あるいは `openlitespeed --dev-https on` は、パソコン上に認証局を作成し、あなたのアカウントでそれを信頼するよう Windows に依頼して、各ローカル サイトを `https://name.test/` で配信します。この認証局は名前制約（Name Constraints）により `.test`、`.localhost`、`.internal` の名前と `127.0.0.1`、`::1` に限定されているため、ほかのサイトの証明書には署名できません。オフにすると Windows の信頼ストアから削除されます。

## PHP

`lsphp.exe` は Windows 向けにビルドした PHP LiteSpeed SAPI で、PHP 8.2 から 8.5 のスレッドセーフ版と非スレッドセーフ版があります。[windows.php.net](https://windows.php.net) の公式 PHP ビルドの上で動作するため、既存の拡張機能と `php.ini` の設定はそのまま使えます。ある PHP バージョンで読み込めない場合、OpenLiteSpeed は FastCGI 経由の `php-cgi.exe` に切り替えます。

<img src="../phpinfo.png" width="640" alt="Windows 上の PHP 8.4 の phpinfo。Server API は LiteSpeed">

PHP バージョンは、ダッシュボード、トレイ メニュー、または `openlitespeed --set-php <folder>` で切り替えます。

PHP ページの **Install PHP** には、windows.php.net にある 7.4 から 8.5 までのスレッドセーフ版と非スレッドセーフ版が一覧表示されます。ダウンロードしたファイルは windows.php.net が公開している SHA-256 で検証され、`php\<version>` に展開され、一般的な拡張機能を有効にした `php.ini` が用意されます。8.2 以降は LSAPI で、それより古いバージョンは FastCGI で動作します。

サイトごとに別の PHP バージョンを使えます: Sites ページの PHP 列で選ぶか、`openlitespeed --site php <site> <folder>` を実行します。指定のないサイトは既定のバージョンを使います。各バージョンの **More** メニューには、その PHP の拡張機能ごとにスイッチがある **Extensions** と、メモリ上限、アップロードと POST のサイズ、実行時間、入力変数、エラー表示、タイムゾーンを設定する **Settings** があります。**Edit php.ini** はファイルをメモ帳で開きます。変更すると Web サーバーがリロードされます。

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../panel-php-dark.png">
  <img src="../panel-php-light.png" width="880" alt="ダッシュボードの PHP ページ。3 つの PHP バージョン、既定のバージョン、各バージョンを使うサイト、Composer">
</picture>

Composer の下の **Install** は、getcomposer.org から `composer.phar` をダウンロードし、SHA-256 で検証します。ダッシュボードのターミナルでは `composer` が `PATH` に入っており、Laravel サイトもこれで作成されます。

## Xdebug

PHP ページの Xdebug 列にあるスイッチ、または `openlitespeed --php xdebug <folder> on` で、その PHP バージョンの Xdebug をオンにします。`ext` フォルダーから読み込める Xdebug がすでにある場合はそれを使います。ない場合は、その PHP バージョン用の公式 Windows ビルドを xdebug.org からダウンロードし、OpenLiteSpeed が保持する SHA-256 で検証します: PHP 8.2 から 8.5 向けの Xdebug 3.5.3（スレッドセーフ版と非スレッドセーフ版）です。

Xdebug はすべてのリクエストで開始し、`127.0.0.1:9003` のデバッガーに接続します。デバッガーがない場合、リクエストはすぐに続行され、ログには何も書き込まれません。Xdebug がオンの間は opcache JIT がオフになり、Xdebug をオフにすると元に戻ります。設定は `php.ini` の末尾に保存され、反映のために Web サーバーがリロードされます。

OpenLiteSpeed を Windows サービスとしてインストールしている間は、Xdebug をオンにできません。パソコン上でポート 9003 を待ち受けるプログラムはどれでもデバッグ セッションを受け取り、サービス アカウントとして PHP コードを実行できるため、サービスをインストールする前に Xdebug をオフにしてください。

**VS Code:** Sites ページの **More > Open in VS Code** は、VS Code がインストールされていればサイトのフォルダーを VS Code で開きます。たとえば PHP Debug 拡張機能でポート 9003 を待ち受ける PHP のデバッグ セッションを開始し、ブラウザーでサイトを開きます。

## メール キャッチャー

Mail ページのスイッチ、または `openlitespeed --mail on` で、ローカル サイトが送信するすべてのメールを受け止めるので、メールがパソコンの外に出ることはありません。PHP の `mail()` は、OpenLiteSpeed が実行する PHP バージョンの `php.ini` にある `sendmail_path` を通じて OpenLiteSpeed に届きます。SMTP で送信するアプリは、ホスト `127.0.0.1`、ポート 1025、暗号化なし、任意のユーザー名とパスワードを使います。新しい Laravel サイトでは、これらの設定が `.env` に書き込まれます。Web リクエストのように始まる接続は閉じられるため、ブラウザーで開いた Web ページからメール キャッチャーにメールを送ることはできません。

メールは、あなたのアカウントだけが読める `data\mail` に、最新の 1000 件、512 MB まで保存されます。Mail ページでは各メッセージをテキスト パートと HTML パートで表示し、添付ファイルを保存できます。HTML パートはスクリプトなしで表示され、インターネットから何も読み込みません。

## バックグラウンド ジョブ

サイトの **More** メニューにある **Background jobs**、または `openlitespeed --job add <site> <name> <command>` は、`php artisan queue:work`、`php artisan schedule:work`、`npm run dev` などの長時間実行されるコマンドをサイトのフォルダーで実行します。その際、そのサイトの PHP と Node.js のバージョンが `PATH` の先頭に置かれます。ジョブは Web サーバーとともに開始、停止します。終了したジョブは 1 秒、2 秒、4 秒と間隔を延ばし、最大 60 秒後に再起動されます。10 分間に 10 回再起動すると停止したままになります。ジョブを停止すると、そのジョブが起動したすべてのプロセスが終了します。出力は `logs\jobs\<site>.<name>.log` に書き込まれ、Logs ページに表示されます。Windows サービスを使う場合、ジョブとメール キャッチャーは PHP と同じくサービス アカウントで実行されます。

## Node.js

Node.js ページの **Install Node.js** には、nodejs.org の Current 版と LTS 版が一覧表示されます。ダウンロードしたファイルは `SHASUMS256.txt` で検証され、`node\<version>` に展開されます。このページには、`Program Files`、Laragon、nvm、Volta、`PATH` で見つかった Node.js も表示されます。**Make default** で Node.js アプリを実行するバージョンを選ぶと、その下にアプリが **Restart** と **Log** 付きで一覧表示されます。

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../panel-node-dark.png">
  <img src="../panel-node-light.png" width="880" alt="ダッシュボードの Node.js ページ。インストール済みの Node.js バージョンと実行中のアプリ">
</picture>

Node.js アプリを変更する必要はありません: `app.listen(3000)` はサーバーが割り当てるアドレスに付け替えられます。[docs/APPS.md](../APPS.md) を参照してください。

サイトごとに別の Node.js を使えます: Sites ページの Version 列で選ぶか、`openlitespeed --site node <site> <node.exe>` を実行します。指定がない場合、`.nvmrc` または `.node-version` ファイルに `20` のようなメジャー バージョンが書かれていれば、インストール済みの最新の Node.js 20 が選ばれ、それ以外は既定のバージョンでサイトを実行します。サイトのバックグラウンド ジョブとターミナルも同じバージョンを使います。

## Apache からの移行

OpenLiteSpeed は既存の Apache 設定を読み込み、各 `<VirtualHost>` に対してサイトを 1 つ作成します。ドメイン、ドキュメント ルート、SSL 証明書、リダイレクト、リライト ルール、エイリアス、プロキシ、アクセス ルール、パスワード保護も引き継ぎます。`.htaccess` ファイルのルールは OpenLiteSpeed 自身が適用するため、そのまま動作します。

```bat
bin\openlitespeed.exe --import-apache --dry-run
bin\openlitespeed.exe --import-apache C:\Apache24\conf\httpd.conf
bin\openlitespeed.exe -s reload
```

パスを指定しない場合は、Laragon、XAMPP、WampServer、または `C:\Apache24` の Apache 設定を自動で見つけます。`--dry-run` は何も変更せずにレポートを表示します。もう一度インポートすると前回のインポートが置き換えられ、`--import-apache --remove` で取り消せます。変換できないルールはレポートに一覧表示されます。アクセス ルールは正確に、またはより厳しく変換され、緩くなることはありません。唯一の例外は、ホスト名や環境変数で特定のクライアントをブロックするルールで、これは警告として報告されます。詳しくは [docs/APACHE.md](../APACHE.md) を参照してください。

## nginx からの移行

`--import-nginx` は nginx に対して同じことを行います: 各 `server` ブロックが、ドメイン、リスナー、SSL 証明書、location、`try_files`、リライト、リダイレクト、エイリアス、プロキシ、アクセス ルール、パスワード保護を持つサイトになり、`fastcgi_pass` の背後の PHP は OpenLiteSpeed の PHP で実行されます。

```bat
bin\openlitespeed.exe --import-nginx --dry-run
bin\openlitespeed.exe --import-nginx C:\nginx\conf\nginx.conf
```

詳しくは [docs/NGINX.md](../NGINX.md) を参照してください。

## 複数の CPU コア

OpenLiteSpeed は CPU コアごとに HTTP ワーカーを 1 つ起動し、既定では最大 8 個です。WebAdmin の **Server Configuration > General** にある **Number of Workers**、または `conf\httpd_config.conf` の `httpdWorkers` を、1 から 32 の任意の値に設定できます。各ワーカーは専用の PHP プールを持ちます。Windows でワーカーが待ち受けソケットをどのように共有するかは、[docs/ARCHITECTURE.md](../ARCHITECTURE.md) を参照してください。

## コマンドライン

コマンドは、インストール先フォルダーで `bin\openlitespeed.exe` として実行します。

| コマンド | 説明 |
|---|---|
| `openlitespeed -s start\|stop\|restart\|reload\|status` | サーバーを制御します。`reload` はダウンタイムなしの再起動です |
| `openlitespeed -d` | フォアグラウンドで実行します |
| `openlitespeed --install-service` / `--uninstall-service` | Windows サービスを登録または削除します |
| `openlitespeed --set-php <folder>` | 別の PHP バージョンを使います |
| `openlitespeed --import-apache [httpd.conf] [--dry-run]` / `--import-apache --remove` | Apache のサイトをインポート / インポートしたサイトを削除します |
| `openlitespeed --import-nginx [nginx.conf] [--dry-run]` / `--import-nginx --remove` | nginx のサイトをインポート / インポートしたサイトを削除します |
| `openlitespeed --acme-install [--email <address>]` | Let's Encrypt 証明書の自動取得をオンにします |
| `openlitespeed --modsec-install [--detection-only]` / `--modsec-install --remove` | OWASP Core Rule Set を使う ModSecurity ファイアウォールをオンまたはオフにします |
| `openlitespeed --dev-sites <folder> [tld]` | フォルダー内の各サブフォルダーにサイトを作成します |
| `openlitespeed --new-site <name> [--wordpress\|--laravel\|--node\|--python]` | プロジェクト フォルダーにサイトを作成します: PHP サイト、データベース付きの WordPress、Laravel プロジェクト、Node.js アプリ、Python アプリ |
| `openlitespeed --site php <site> <folder>\|default` | 1 つのサイトで別の PHP バージョンを使います |
| `openlitespeed --site app <site> node\|python\|off` | サイトのフォルダーを Node.js または Python アプリとして実行するか、PHP サイトに戻します |
| `openlitespeed --site node <site> <node.exe>\|default` | 1 つのサイトで別の Node.js バージョンを使います |
| `openlitespeed --php list` | このパソコンの PHP バージョンを、各バージョンのサイトと Xdebug の状態とともに JSON で表示します |
| `openlitespeed --php available` / `install <8.x> [ts]` / `remove <folder>` | windows.php.net の PHP バージョンを一覧表示してインストールし、ダウンロードしたものを削除します |
| `openlitespeed --php ext <folder>` / `ext-set <folder> <name> on\|off` | PHP バージョンの拡張機能を一覧表示し、オンまたはオフにします |
| `openlitespeed --php ini <folder>` / `ini-set <folder> <key> <value>` | よく使う `php.ini` の設定を読み取るか変更します |
| `openlitespeed --php xdebug <folder> on\|off\|status` | PHP バージョンの Xdebug をオンまたはオフにするか、オンかどうかを表示します |
| `openlitespeed --node list\|available` / `install <major>` / `default <node.exe>` / `remove <node.exe>` | アプリ用の Node.js バージョンを管理します |
| `openlitespeed --composer install` | Composer を `tools\composer` にダウンロードします |
| `openlitespeed --set-port http\|admin <port>` | Web サイトまたは WebAdmin のポートを変更します |
| `openlitespeed --proxy add <name> <port>` / `remove <name>` / `list` | ローカル ポートのアプリ用プロキシ サイトを追加または削除します |
| `openlitespeed --dev-https on [port]\|off\|status` | ローカル サイト向けの信頼された HTTPS |
| `openlitespeed --dev-hosts` / `--dev-hosts-clear` | hosts ファイルにサイト名を追加または削除します |
| `openlitespeed --db install\|fetch\|start\|stop\|status` | MariaDB と phpMyAdmin をインストール、ダウンロードのみ、起動、停止します |
| `openlitespeed --db list` / `create <name> [--no-user]` / `drop <name> [--user]` | データベースを一覧表示、作成、削除します |
| `openlitespeed --db export <name> <file.sql>` / `import <name> <file.sql>` | データベースをファイルにエクスポートするか、ファイルをデータベースにインポートします |
| `openlitespeed --tunnel start <site>` / `stop <site>` / `stop --all` / `status` | Cloudflare Tunnel でサイトを公開するか、公開を停止します |
| `openlitespeed --tunnel token` / `run` / `run-stop` / `token clear` | 標準入力からトンネル トークンを保存し、独自ドメインを接続または切断し、トークンを消去します |
| `openlitespeed --mail on\|off\|status` | メール キャッチャーをオンまたはオフにするか、動作中かどうかを表示します |
| `openlitespeed --mail list` / `remove <id>` / `save <id> <n> <file>` / `clear` | 受け止めたメールの JSON での一覧表示、1 通の削除、メッセージの `n` 番目（0 から）の添付ファイルのファイルへの保存、すべてのメッセージの削除を行います |
| `openlitespeed --mail sendmail [-t] [-i] [-f <from>] [<recipient>...]` | sendmail と同じように、標準入力からメッセージを保存します |
| `openlitespeed --job list` / `add <site> <name> <command>` / `remove <site> <name>` | サイトのバックグラウンド ジョブを一覧表示、追加、削除します |
| `openlitespeed --job start\|stop\|restart <site> <name>` / `autostart <site> <name> on\|off` | ジョブを制御し、Web サーバーと一緒に開始するかどうかを選びます |
| `openlitespeed --status` | サーバー、PHP、サイト、データベース、トンネルの状態を JSON で表示します |
| `openlitespeed --admin-password admin\conf\htpasswd <password>` | WebAdmin のパスワードを設定します |

トレイ アプリは、サーバーの起動、停止、リロード、サイト、WebAdmin、ログを開く、サイトとプロキシ サイトの作成、信頼された HTTPS のオンとオフ、PHP バージョンの切り替え、Apache と nginx のサイトのインポートを行います。フルスタック エディションでは、ダッシュボードを開いたり MariaDB を制御したりもできます。1 日に 1 回 GitHub で新しいリリースを確認し、**Update** でダウンロード、署名とチェックサムの検証、新しいセットアップの実行を行います。`conf\olstray.ini` の `[update]` に `check=0` を設定すると、確認をオフにできます。

## ソースからのビルド

[docs/BUILD.md](../BUILD.md) を参照してください。ビルドには Visual Studio 2022、CMake、Ninja を使い、すべての依存関係を固定したコミットで取得します。

## 制限事項

- バーチャルホストごとのユーザー切り替え（suEXEC）はありません。PHP はサーバーを実行しているユーザー、またはサービス アカウントで実行されます。
- `SO_REUSEPORT` はありません。ワーカーはアドレスごとに 1 つの待ち受けソケットを共有します。
- cgroups、namespaces、bubblewrap、CageFS は Linux の機能のため、利用できません。
- App Server コンテキストによる Ruby アプリケーションはサポートしていません。

## コントリビューション

バグ報告とプルリクエストを歓迎します。[CONTRIBUTING.md](../../CONTRIBUTING.md) を参照してください。セキュリティ上の問題は、[SECURITY.md](../../SECURITY.md) に記載の方法で非公開で報告してください。

## ライセンス

GPL-3.0 です。[LICENSE](../../LICENSE) を参照してください。OpenLiteSpeed は LiteSpeed Technologies が開発し、GPL-3.0 でライセンスされています。`lsphp.exe` は PHP のソースからビルドされ、[PHP License](../../win/lsphp/PHP-LICENSE.txt) が適用されます。サードパーティ コンポーネントは [THIRD-PARTY-NOTICES.txt](../../win/package/THIRD-PARTY-NOTICES.txt) に記載しています。

このプロジェクトは LiteSpeed Technologies および PHP Group とは無関係であり、両者の承認を受けたものではありません。LiteSpeed と OpenLiteSpeed は LiteSpeed Technologies, Inc. の商標です。

同じ作者によるプロジェクト: [OpenLiteSpeed for cPanel and WHM](https://github.com/NeuralFoundry/openlitespeed-cpanel)。
