[English](../../README.md) | [Türkçe](README.tr.md) | [简体中文](README.zh-CN.md) | [Español](README.es.md) | [Português](README.pt-BR.md) | [Русский](README.ru.md) | [日本語](README.ja.md) | [Deutsch](README.de.md) | [Français](README.fr.md) | **한국어** | [Bahasa Indonesia](README.id.md)

# OpenLiteSpeed for Windows

웹사이트: [openlitespeed.dev/windows](https://openlitespeed.dev/windows/)

Windows용 OpenLiteSpeed는 [OpenLiteSpeed](https://openlitespeed.org) 1.9.4 웹 서버의 네이티브 Windows 빌드입니다. WSL, Docker, 가상 머신 없이 Windows 10, Windows 11, Windows Server에서 실행되며, PHP를 LSAPI 또는 FastCGI로 실행하고, 단독으로 설치하거나 Laragon과 XAMPP 안에 설치할 수 있습니다. 풀 스택 에디션은 대시보드, MariaDB, phpMyAdmin, Cloudflare Tunnel을 추가로 제공하므로 로컬 IP 뒤에 있는 컴퓨터의 사이트도 인터넷에 공개할 수 있습니다. 대시보드에서 PHP와 Node.js 버전을 설치하고, 사이트마다 PHP 버전을 따로 지정하고, WordPress, Laravel, Node.js, Python 사이트를 만들고, 데이터베이스를 관리할 수 있습니다. GPL-3.0 라이선스의 무료 오픈 소스입니다.

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../panel-dark.png">
  <img src="../panel-light.png" width="880" alt="Windows용 OpenLiteSpeed 대시보드 개요: 웹 서버, MariaDB, Cloudflare Tunnel, 실시간 요청 수, 로컬 사이트">
</picture>

## 기능

- Visual C++로 빌드한 네이티브 Win32 프로그램으로, 그래픽 설치 프로그램, 트레이 앱, Windows 서비스를 제공합니다
- 하나의 설치 프로그램에 두 가지 에디션: 웹 서버만 설치하거나, 대시보드, MariaDB, phpMyAdmin, Cloudflare Tunnel을 포함한 풀 스택을 설치합니다
- 대시보드: 서비스 시작과 중지, 사이트 생성, PHP와 Node.js 설치, 데이터베이스 관리, 로그 확인, 포트 변경, 사이트 공개를 하나의 창에서 처리합니다
- PHP 7.4부터 8.5까지와 Node.js LTS 버전을 대시보드에서 설치하며, 공개된 SHA-256으로 검증합니다
- 사이트별 PHP 버전, 스위치로 켜고 끄는 PHP 확장, 자주 쓰는 `php.ini` 설정을 위한 양식
- 새 사이트 유형: 일반 PHP 사이트, WordPress, Laravel(Composer 포함), Node.js 앱, Python 앱
- 데이터베이스: 전용 사용자가 있는 데이터베이스 생성, `.sql` 파일 내보내기와 가져오기, 데이터베이스 삭제
- Cloudflare Tunnel로 로컬 사이트 공개: 포트 포워딩이나 공인 IP 없이 컴퓨터의 사이트에 공개 https 주소를 제공합니다. 터널 토큰을 사용하면 자체 도메인도 연결할 수 있습니다.
- MariaDB 11.8과 phpMyAdmin 5.2는 127.0.0.1에서만 동작하며, root 비밀번호는 자동으로 생성됩니다. 새 WordPress 사이트마다 전용 데이터베이스와 사용자가 만들어집니다.
- 모든 CPU 코어 활용: 같은 포트에서 최대 32개의 HTTP 워커 프로세스가 동작하고, 충돌 후 자동으로 다시 시작되며, 다운타임 없이 다시 로드됩니다
- LSAPI(Windows용 `lsphp.exe` 포함) 또는 FastCGI(`php-cgi.exe`)를 통한 PHP 8.2, 8.3, 8.4, 8.5
- 설치된 PHP 버전, Laragon, XAMPP를 찾아 자동으로 설정합니다
- `httpd.conf`에서 Apache 사이트 가져오기: 가상 호스트, SSL 인증서, 리디렉션, 별칭, 프록시, CGI, 액세스 규칙, 비밀번호 보호와 함께 `.htaccess` 파일의 규칙까지 가져옵니다
- CGI 프로그램: `.exe`, `.bat`, `.cmd` 파일과 Perl이나 Python처럼 `#!` 줄이 있는 스크립트를 각각 제한된 토큰을 가진 별도의 작업 개체(job object)에서 실행합니다
- 서버가 시작하고 다시 시작하는 Node.js와 Python(WSGI) 애플리케이션. Node.js 앱은 [openlitespeed-node](https://github.com/NeuralFoundry/openlitespeed-node) 덕분에 수정 없이 실행되며 WebSocket도 지원합니다. [docs/APPS.md](../APPS.md)를 참고하세요
- 로컬 개발 사이트: 프로젝트 폴더 안의 모든 폴더가 `http://folder.test/`가 됩니다. 트레이나 대시보드에서 새 사이트를 만들 수 있고, 원하면 WordPress도 함께 설치됩니다
- 로컬 사이트를 위한 신뢰할 수 있는 HTTPS: 컴퓨터에서 만든 인증 기관으로, `.test`, `.localhost`, `.internal` 이름과 컴퓨터 자신의 주소에만 서명할 수 있습니다
- 로컬 포트에서 실행되는 앱을 위한 프록시 사이트. 예를 들어 `127.0.0.1:3000`에서 실행되는 앱에 `api.test`를 연결합니다
- 서명된 업데이트: 트레이 앱이 새 릴리스를 내려받아 서명을 확인하고, 사용자가 확인하면 설치합니다
- HTTP/2, HTTP/3(QUIC), TLS 1.3, Brotli, LSCache, WebAdmin 콘솔
- 내장 ACME 클라이언트가 자동으로 발급하고 갱신하는 Let's Encrypt 무료 HTTPS 인증서. [docs/ACME.md](../ACME.md)를 참고하세요
- 내장 모듈: LSCache, 업로드 진행률, 요청 본문 파싱, LuaJIT 기반 Lua 스크립팅. [docs/MODULES.md](../MODULES.md)를 참고하세요
- `.htaccess` 파일이 Apache와 똑같이 동작합니다: 재작성 규칙, 액세스 규칙(`Require`, `Order`, `Allow`, `Deny`), 비밀번호, `<Files>` 섹션, `Options Indexes`, `php_flag engine off`가 지원되며, 파일이 바뀌면 1초 안에 적용됩니다
- 웹 애플리케이션 방화벽: OWASP Core Rule Set을 사용하는 ModSecurity 3을 명령 하나로 켤 수 있습니다. [docs/PROTECTION.md](../PROTECTION.md#web-application-firewall)를 참고하세요
- 봇 차단: reCAPTCHA 또는 hCaptcha 챌린지, 클라이언트 IP별 요청, 대역폭, 연결 수 제한, 그리고 PHP와 재작성 규칙에서 쓸 수 있도록 MaxMind GeoIP2 또는 IP2Location 데이터베이스로 방문자의 국가를 제공합니다. [docs/PROTECTION.md](../PROTECTION.md)를 참고하세요
- WordPress용 LiteSpeed Cache를 바로 사용할 수 있습니다: 플러그인을 설치하고 활성화하기만 하면 되며, 서버 설정은 필요 없습니다. 콘텐츠가 바뀌면 페이지 캐시가 자동으로 삭제됩니다.
- Windows에 맞춘 보안 강화: 대소문자, 짧은 이름, 데이터 스트림, 장치 이름을 악용하는 수법에 대해 파일 이름을 검사하고, PHP는 제한된 토큰으로, 서비스는 가상 계정으로 실행합니다

## 성능: Windows용 OpenLiteSpeed와 nginx, Apache 비교

이 저장소의 네이티브 Windows 빌드를 같은 Windows 11 컴퓨터에서 nginx, Apache와 비교했습니다. PHP는 8.4이며, 모든 서버에 같은 12개의 CPU 코어를 할당했습니다. 값은 초당 요청 수이며, 높을수록 좋습니다. 모든 실행이 오류 없이 끝났습니다.

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../bench-windows-overview-dark.svg">
  <img src="../bench-windows-overview-light.svg" width="880" alt="Windows용 OpenLiteSpeed와 nginx, Apache 비교: 정적 파일, PHP 애플리케이션, WordPress의 초당 요청 수">
</picture>

| 서버 | 정적 파일 | PHP 애플리케이션 | WordPress, 캐시 없음 | HTTPS를 통한 PHP | 512개 연결에서의 PHP |
|---|--:|--:|--:|--:|--:|
| OpenLiteSpeed + LSAPI | 175,203 | 9,548 | 117 | 8,687 | 9,434 |
| OpenLiteSpeed + FastCGI |  | 9,538 | 119 | 8,961 | 9,587 |
| nginx + PHP-CGI | 60,306 | 2,949 | 122 | 472 | 462 |
| Apache + mod_php | 17,889 | 7,532 | 105 | 4,473 | 3,078 |
| Apache + PHP-CGI |  | 5,743 | 118 | 4,356 | 5,396 |

정적 파일은 PHP를 거치지 않으므로 서버마다 한 번만 측정했습니다. 정적 파일, PHP 애플리케이션, WordPress: 128개의 keep-alive 연결(WordPress는 64개)로 각각 20초 동안 실행했습니다. HTTPS: 10초, 128개 연결. 마지막 열은 512개 연결로 60초 동안 계속한 부하 테스트의 평균입니다:

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../bench-windows-stress-app-dark.svg">
  <img src="../bench-windows-stress-app-light.svg" width="880" alt="Windows용 OpenLiteSpeed 부하 테스트: 512개 연결로 60초 동안 10초마다 측정한 초당 요청 수">
</picture>

캐시를 쓰지 않는 WordPress는 PHP와 MySQL이 대부분의 일을 하기 때문에 어느 서버에서나 속도가 비슷합니다. 페이지 캐시를 쓰면 결과가 달라집니다. 같은 사이트의 홈 페이지, 64개 연결:

| 서버와 캐시 | 요청/초 | p99 |
|---|--:|--:|
| OpenLiteSpeed + LiteSpeed Cache 플러그인 | 21,180 | 6.1 ms |
| Apache + mod_cache_disk | 18,362 | 6.1 ms |
| nginx + fastcgi_cache | 7,745 | 10.8 ms |

지연 시간, 메모리, 전체 테스트 환경, 원시 데이터는 [docs/BENCHMARKS.md](../BENCHMARKS.md)에 있습니다.

## 요구 사항

- Windows 10, Windows 11 또는 Windows Server 2019 이상, x64
- Windows용 PHP 8.2 이상, x64. 설치 프로그램이 내려받을 수 있습니다.

## 설치

[Releases](https://github.com/NeuralFoundry/openlitespeed-windows/releases/latest)에서 `OpenLiteSpeed-1.9.4-win64-setup.exe`를 내려받아 실행합니다. 설치 프로그램은 찾은 PHP 버전을 보여 주고, 설치 유형을 제안하고, 에디션을 묻습니다.

<img src="../setup.png" width="576" alt="에디션 선택과 PHP 버전이 표시된 Windows용 OpenLiteSpeed 설치 화면">

| 에디션 | 포함 항목 |
|---|---|
| 풀 스택 | OpenLiteSpeed, PHP, 트레이 앱, 대시보드, MariaDB, phpMyAdmin, Cloudflare Tunnel. 설치 프로그램은 약 170 MB를 내려받고 모든 파일을 고정된 SHA-256으로 검증합니다. |
| 웹 서버 전용 | OpenLiteSpeed, PHP, 트레이 앱(새 사이트, 신뢰할 수 있는 HTTPS, PHP 전환, 가져오기, 업데이트 포함) |

설치 프로그램을 다시 실행하면 에디션을 바꿀 수 있습니다. 웹 서버 전용으로 바꿔도 데이터베이스는 유지됩니다. 풀 스택 에디션은 트레이 앱과 함께 실행되므로 Windows 서비스 옵션은 웹 서버 전용 에디션에서만 쓸 수 있습니다.

설치 프로그램은 설치 폴더에 대한 접근을 사용자 계정, 관리자, 시스템으로 제한하므로 컴퓨터의 다른 사용자는 프로그램이나 설정을 바꿀 수 없습니다. Laragon과 XAMPP의 폴더는 바꾸지 않는데, 이들의 PHP 폴더는 다른 사용자도 바꿀 수 있는 경우가 많습니다. Windows 서비스를 사용하면 이를 통해 다른 사용자가 서비스 계정으로 코드를 실행할 수 있으므로, 서비스를 설치할 때 이런 PHP 폴더에 대해 경고합니다.

| 설치 유형 | 설치 폴더 | 프로젝트 폴더 | 시작 방법 |
|---|---|---|---|
| 독립 실행형 서버 | `C:\OpenLiteSpeed` | `C:\OpenLiteSpeed\www` | 트레이 앱 또는 Windows 서비스 |
| Laragon 내부 | `C:\laragon\bin\openlitespeed` | Laragon 문서 루트 | Laragon **Start All** |
| XAMPP 내부 | `C:\xampp\openlitespeed` | `C:\xampp\htdocs` | 트레이 앱 또는 Windows 서비스 |

웹사이트는 포트 80에서 응답하며, 포트 80이 사용 중이면 8088을 사용합니다. WebAdmin은 `https://localhost:7080/`에 있으며, 사용자 `admin`과 설치가 끝날 때 표시되는 비밀번호로 로그인합니다. 기존 설치 위에 설치 프로그램을 다시 실행하면 설정을 유지한 채 업그레이드합니다.

<img src="../webadmin.png" width="880" alt="Windows에서 실행 중인 OpenLiteSpeed WebAdmin 콘솔">

무인 설치:

```bat
OpenLiteSpeed-1.9.4-win64-setup.exe --silent --mode standalone --download-php 8.4 --edition full
```

| 옵션 | 설명 |
|---|---|
| `--mode standalone\|laragon\|xampp` | 설치 유형. 생략하면 자동으로 감지합니다 |
| `--edition server\|full` | 에디션. 생략하면 새로 설치할 때는 `server`, 업그레이드할 때는 현재 에디션입니다. `--service`에는 `server`가 필요합니다 |
| `--php <folder>` / `--download-php 8.2\|8.3\|8.4\|8.5` | 사용할 PHP |
| `--dir <folder>` / `--www <folder>` | 설치 폴더 / 프로젝트 폴더 |
| `--port <n>` / `--admin-port <n>` | 웹사이트 포트와 WebAdmin 포트 |
| `--service` / `--no-tray` | Windows 서비스 설치 / 트레이 앱을 시작하지 않음 |
| `--log <file>` | 로그 파일. 기본값은 `%TEMP%\OpenLiteSpeed-setup.log`이며 관리자 비밀번호가 포함됩니다 |

스크립트 배포용으로 같은 파일을 `install.ps1`이 포함된 zip으로도 제공합니다. 설치 프로그램과 마찬가지로, 설치 폴더가 아직 상위 폴더의 권한을 상속하고 있으면 접근을 사용자 계정, 관리자, 시스템으로 제한합니다:

```powershell
powershell -ExecutionPolicy Bypass -File install.ps1 -Mode standalone -DownloadPhp 8.4 -Edition full
```

제거하려면 **설정 > 앱**을 사용하거나 설치 폴더에서 `uninstall.exe --uninstall --silent`를 실행합니다. 프로젝트 폴더와 데이터베이스가 들어 있는 `data` 폴더는 남습니다.

## 대시보드

풀 스택 에디션에서는 시작 메뉴에서 또는 트레이 아이콘을 두 번 클릭해 대시보드를 엽니다. 웹 페이지가 아니라 데스크톱 창이므로 어떤 포트에서도 수신 대기하지 않습니다. 페이지는 다음과 같습니다:

| 페이지 | 할 수 있는 일 |
|---|---|
| Overview | 웹 서버, MariaDB, Cloudflare Tunnel 시작과 중지, 초당 요청 수와 열린 연결 확인 |
| Sites | 사이트 생성과 삭제, 사이트별 PHP 또는 Node.js 버전 선택, 사이트 공개, 사이트 폴더 열기 또는 그 안에서 터미널이나 VS Code 열기, 폴더를 Node.js 또는 Python 앱으로 실행, 백그라운드 작업 실행 |
| PHP | PHP 버전 설치, 기본 버전 선택, 확장과 Xdebug 켜고 끄기, `php.ini` 설정 변경, Composer 설치 |
| Node.js | Node.js 버전 설치, 앱을 실행할 버전 선택, 앱 다시 시작과 로그 확인 |
| Databases | 데이터베이스와 사용자 생성, `.sql` 파일 내보내기와 가져오기, 데이터베이스 삭제, phpMyAdmin 열기 |
| Mail | 메일 캐처 켜고 끄기, 수집한 메일을 HTML 부분과 첨부 파일과 함께 확인 |
| Logs | 서버, MariaDB, 앱, 터널 로그 확인, 줄 필터링, 새 줄 실시간 추적 |
| Settings | Windows와 함께 시작, 프로젝트 폴더, 신뢰할 수 있는 HTTPS, HTTP 포트와 WebAdmin 포트, WebAdmin 비밀번호 |

왼쪽 아래의 **Terminal**은 기본 PHP, Composer, Node.js, MariaDB 클라이언트가 `PATH`에 들어 있는 명령 프롬프트를 엽니다.

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../panel-sites-dark.png">
  <img src="../panel-sites-light.png" width="880" alt="대시보드의 Sites 페이지: 사이트별 PHP 버전, Node.js와 Python 앱, 프록시 사이트, Online 스위치">
</picture>

대시보드에는 Microsoft Edge WebView2 Runtime이 필요하며, Windows 11과 최신 Windows 10에는 기본으로 포함되어 있습니다.

## 로컬 사이트

프로젝트 폴더 안의 모든 폴더는 `http://folder.test/` 주소의 사이트입니다. 대시보드나 트레이의 **New site**가 폴더를 만듭니다. 대시보드에서 만들 수 있는 사이트 유형은 다음과 같습니다:

| 유형 | 만들어지는 것 |
|---|---|
| PHP 사이트 | `index.php`가 들어 있는 폴더 |
| WordPress | 최신 WordPress. MariaDB가 설치되어 있으면 사이트용 데이터베이스와 사용자, 미리 채워진 `wp-config.php`도 만들어지므로 WordPress 설치 화면에서는 사이트 제목과 관리자 계정만 입력하면 됩니다 |
| Laravel | `composer create-project`로 만든 새 Laravel 프로젝트. `public` 폴더에서 서비스됩니다 |
| Node.js 앱 | 간단한 `server.js`와 `package.json`. 서버가 기본 Node.js로 시작합니다 |
| Python 앱 | WSGI `wsgi.py`. 서버가 컴퓨터에서 찾은 Python으로 시작합니다 |

WordPress 설치 화면은 이 컴퓨터에서만 열리므로 네트워크의 다른 사람이 먼저 설치를 끝낼 수 없습니다. `package.json`이 있는 폴더는 **More > Run as a Node.js app**으로 Node.js 앱이 되고, **Run as a PHP site**로 되돌릴 수 있습니다. **Remove site**는 폴더를 휴지통으로 옮기고 데이터베이스는 유지합니다.

**New proxy site**는 로컬 포트에서 수신 대기하는 앱에 고유한 이름을 붙입니다. 예를 들어 포트 5173의 Vite 개발 서버에 `app.test`를 붙일 수 있습니다. WebSocket도 그대로 전달됩니다. 프록시 사이트는 그 뒤에 있는 앱과 마찬가지로 이 컴퓨터에만 응답합니다.

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../panel-site-dark.png">
  <img src="../panel-site-light.png" width="420" alt="사이트 이름과 사이트 유형(PHP, WordPress, Laravel, Node.js 앱, Python 앱)이 있는 New site 대화 상자">
</picture>
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../panel-proxy-dark.png">
  <img src="../panel-proxy-light.png" width="420" alt="사이트 이름과 앱 포트가 있는 New proxy site 대화 상자">
</picture>

## 사이트를 인터넷에 공개하기

대시보드에서 사이트 옆의 **Online**을 켜거나 `openlitespeed --tunnel start <site>`를 실행합니다. Cloudflare가 사이트에 임의의 `https://*.trycloudflare.com` 주소를 부여하며, 끌 때까지 누구나 이 주소로 접속할 수 있습니다. 연결이 컴퓨터에서 바깥으로 나가기 때문에 NAT, 공유기, 로컬 IP 뒤에서도 동작하며, 방화벽에서 아무것도 열 필요가 없습니다. 로그와 PHP에는 방문자의 실제 IP 주소가 그대로 남습니다. 공개 주소는 로컬 주소 아래에 표시되며, 옆에 **Copy**가 있습니다.

자체 도메인을 쓰려면 Cloudflare Zero Trust에서 터널을 만들고, 공개 호스트 이름이 웹사이트 포트의 `http://127.0.0.1`을 가리키도록 하고 HTTP Host 헤더에 사이트 이름을 지정한 다음, 대시보드의 **Use your own domain**에 터널 토큰을 붙여 넣습니다. 토큰은 Windows 계정용으로 암호화되어 저장됩니다.

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../panel-share-dark.png">
  <img src="../panel-share-light.png" width="420" alt="Cloudflare를 통해 로컬 사이트를 인터넷에 공유하는 대화 상자">
</picture>
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../panel-token-dark.png">
  <img src="../panel-token-light.png" width="420" alt="Cloudflare 터널 토큰으로 자체 도메인을 연결하는 대화 상자">
</picture>

프로젝트 폴더의 사이트와 프록시 사이트만 공개할 수 있습니다. 프록시 사이트를 공유하면 터널이 해당 앱에 바로 연결됩니다. 자체 도메인을 쓸 때는 그 호스트 이름도 앱의 포트를 가리키도록 설정하세요. phpMyAdmin과 WebAdmin은 이 컴퓨터에만 응답하며, 서버가 방문자와 이 컴퓨터를 구분할 수 없으면 터널이 시작되지 않습니다.

## 데이터베이스

MariaDB는 `127.0.0.1`에서만 수신 대기하며, 관리자 권한이 아닌 사용자 계정으로 실행됩니다. 설치 프로그램이 MariaDB를 내려받고, 트레이 앱이 처음 시작될 때 데이터베이스를 만듭니다. root 비밀번호는 이때 생성되어 사용자 계정과 관리자만 읽을 수 있는 `data\client.cnf`에 저장됩니다. `SELECT ... INTO OUTFILE`과 `LOAD DATA`는 `data\files` 안에서만 동작합니다. 대시보드나 트레이의 **Copy password**는 클립보드 기록에 남기지 않고 비밀번호를 클립보드에 복사합니다. phpMyAdmin은 `http://phpmyadmin.test/`에 있으며, 이 컴퓨터에서만 접속할 수 있습니다.

트레이나 대시보드에서 만든 WordPress 사이트에는 전용 데이터베이스와 사용자가 만들어지고 `wp-config.php`도 채워집니다.

**Databases** 페이지에서 **New database**는 같은 이름의 사용자와 자동 생성된 비밀번호로 데이터베이스를 만들며, 비밀번호는 **Copy** 버튼과 함께 한 번만 표시됩니다. **Export**는 `mariadb-dump`로 `.sql` 파일을 쓰고, **Import**는 `.sql` 파일을 데이터베이스로 불러오며, **Remove**는 먼저 이름을 입력하게 하고 사용자도 함께 삭제할 수 있습니다.

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../panel-db-dark.png">
  <img src="../panel-db-light.png" width="880" alt="대시보드의 Databases 페이지: MariaDB, root 비밀번호, 데이터베이스별 내보내기, 가져오기, 삭제">
</picture>

## 신뢰할 수 있는 HTTPS

대시보드나 트레이의 **Trusted HTTPS** 또는 `openlitespeed --dev-https on`은 컴퓨터에 인증 기관을 만들고, 사용자 계정에서 이를 신뢰하도록 Windows에 요청한 다음, 모든 로컬 사이트를 `https://name.test/`로 제공합니다. 이 인증 기관은 이름 제약 조건(name constraints)에 따라 `.test`, `.localhost`, `.internal` 이름과 `127.0.0.1`, `::1`로 제한되므로 다른 사이트의 인증서에는 서명할 수 없습니다. 끄면 Windows 신뢰 저장소에서 제거됩니다.

## PHP

`lsphp.exe`는 Windows용으로 빌드한 PHP LiteSpeed SAPI로, PHP 8.2부터 8.5까지 스레드 안전(thread safe) 버전과 스레드 비안전(non thread safe) 버전을 지원합니다. [windows.php.net](https://windows.php.net)의 공식 PHP 빌드 위에서 동작하므로 기존 확장과 `php.ini` 설정을 그대로 쓸 수 있습니다. 어떤 PHP 버전이 이를 불러오지 못하면 OpenLiteSpeed는 FastCGI를 통한 `php-cgi.exe`로 전환합니다.

<img src="../phpinfo.png" width="640" alt="Windows의 PHP 8.4 phpinfo, Server API LiteSpeed">

PHP 버전은 대시보드, 트레이 메뉴 또는 `openlitespeed --set-php <folder>`로 바꿉니다.

PHP 페이지의 **Install PHP**는 windows.php.net에 있는 7.4부터 8.5까지의 스레드 안전 버전과 스레드 비안전 버전을 보여 줍니다. 내려받은 파일은 windows.php.net이 공개한 SHA-256으로 검증되고, `php\<version>`에 압축이 풀리며, 일반적인 확장이 켜진 `php.ini`가 함께 만들어집니다. 8.2 이상은 LSAPI로, 그보다 오래된 버전은 FastCGI로 실행됩니다.

사이트마다 PHP 버전을 따로 쓸 수 있습니다: Sites 페이지의 PHP 열에서 고르거나 `openlitespeed --site php <site> <folder>`를 실행합니다. 따로 선택하지 않은 사이트는 기본 버전을 따릅니다. 각 버전의 **More** 메뉴에는 해당 PHP의 확장마다 스위치가 있는 **Extensions**와, 메모리 제한, 업로드와 POST 크기, 실행 시간, 입력 변수, 오류 표시, 시간대를 설정하는 **Settings**가 있습니다. **Edit php.ini**는 파일을 메모장에서 엽니다. 변경하면 웹 서버가 다시 로드됩니다.

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../panel-php-dark.png">
  <img src="../panel-php-light.png" width="880" alt="대시보드의 PHP 페이지: 세 개의 PHP 버전, 기본 버전, 버전별 사용 사이트, Composer">
</picture>

Composer 아래의 **Install**은 getcomposer.org에서 `composer.phar`를 내려받아 SHA-256으로 검증합니다. 대시보드 터미널의 `PATH`에는 `composer`가 들어 있으며, Laravel 사이트도 이것으로 만들어집니다.

## Xdebug

PHP 페이지의 Xdebug 열에 있는 스위치나 `openlitespeed --php xdebug <folder> on`으로 해당 PHP 버전의 Xdebug를 켭니다. `ext` 폴더에서 이미 불러오는 Xdebug가 있으면 그것을 씁니다. 없으면 해당 PHP 버전용 공식 Windows 빌드를 xdebug.org에서 내려받아 OpenLiteSpeed에 보관된 SHA-256으로 검증합니다: PHP 8.2부터 8.5까지를 위한 Xdebug 3.5.3이며, 스레드 안전 버전과 스레드 비안전 버전을 지원합니다.

Xdebug는 요청마다 시작되어 `127.0.0.1:9003`의 디버거에 연결합니다. 디버거가 없으면 요청은 곧바로 계속 진행되고 로그에는 아무것도 기록되지 않습니다. Xdebug가 켜져 있는 동안에는 opcache JIT가 꺼지며, Xdebug를 끄면 다시 켜집니다. 설정은 `php.ini` 끝에 저장되며, 적용을 위해 웹 서버가 다시 로드됩니다.

OpenLiteSpeed가 Windows 서비스로 설치되어 있는 동안에는 Xdebug를 켤 수 없습니다. 컴퓨터에서 포트 9003을 수신 대기하는 프로그램은 어떤 것이든 디버그 세션을 받아 서비스 계정으로 PHP 코드를 실행할 수 있으므로, 서비스를 설치하기 전에 Xdebug를 끄세요.

**VS Code:** Sites 페이지의 **More > Open in VS Code**는 VS Code가 설치되어 있으면 사이트 폴더를 VS Code에서 엽니다. 예를 들어 PHP Debug 확장으로 포트 9003에서 수신 대기하는 PHP 디버그 세션을 시작하고, 브라우저에서 사이트를 엽니다.

## 메일 캐처

Mail 페이지의 스위치나 `openlitespeed --mail on`은 로컬 사이트가 보내는 모든 메일을 붙잡아 두므로 메일이 컴퓨터 밖으로 나가지 않습니다. PHP `mail()`은 OpenLiteSpeed가 실행하는 PHP 버전의 `php.ini`에 있는 `sendmail_path`를 통해 OpenLiteSpeed로 전달되며, SMTP로 보내는 앱은 호스트 `127.0.0.1`, 포트 1025, 암호화 없음, 임의의 사용자 이름과 비밀번호를 사용합니다. 새 Laravel 사이트에는 이 설정이 `.env`에 들어갑니다. 웹 요청처럼 시작하는 연결은 닫히므로 브라우저의 웹 페이지는 메일 캐처로 메일을 보낼 수 없습니다.

메일은 사용자 계정만 읽을 수 있는 `data\mail`에 최근 1000개, 최대 512 MB까지 보관됩니다. Mail 페이지는 각 메시지를 텍스트 부분과 HTML 부분으로 보여 주며, 첨부 파일을 저장할 수 있습니다. HTML 부분은 스크립트 없이 표시되며 인터넷에서 아무것도 불러오지 않습니다.

## 백그라운드 작업

사이트의 **More** 메뉴에 있는 **Background jobs** 또는 `openlitespeed --job add <site> <name> <command>`는 `php artisan queue:work`, `php artisan schedule:work`, `npm run dev` 같은 오래 실행되는 명령을 사이트 폴더에서 실행하며, 이때 해당 사이트의 PHP와 Node.js 버전이 `PATH` 맨 앞에 놓입니다. 작업은 웹 서버와 함께 시작하고 멈춥니다. 끝난 작업은 1초, 2초, 4초, 최대 60초 뒤에 다시 시작되며, 10분 안에 10번 다시 시작되면 멈춘 상태로 남습니다. 작업을 멈추면 그 작업이 시작한 모든 프로세스가 종료됩니다. 출력은 `logs\jobs\<site>.<name>.log`에 기록되며 Logs 페이지에 표시됩니다. Windows 서비스로 실행할 때는 PHP와 마찬가지로 서비스 계정이 작업과 메일 캐처를 실행합니다.

## Node.js

Node.js 페이지의 **Install Node.js**는 nodejs.org의 Current 버전과 LTS 버전을 보여 줍니다. 내려받은 파일은 `SHASUMS256.txt`로 검증되고 `node\<version>`에 압축이 풀립니다. 이 페이지에는 `Program Files`, Laragon, nvm, Volta, `PATH`에서 찾은 Node.js도 표시됩니다. **Make default**는 Node.js 앱을 실행할 버전을 고르며, 그 아래에 앱이 **Restart**와 **Log**와 함께 나열됩니다.

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../panel-node-dark.png">
  <img src="../panel-node-light.png" width="880" alt="대시보드의 Node.js 페이지: 설치된 Node.js 버전과 실행 중인 앱">
</picture>

Node.js 앱은 수정할 필요가 없습니다: `app.listen(3000)`은 서버가 지정한 주소로 옮겨집니다. [docs/APPS.md](../APPS.md)를 참고하세요.

사이트마다 Node.js를 따로 쓸 수 있습니다: Sites 페이지의 Version 열에서 고르거나 `openlitespeed --site node <site> <node.exe>`를 실행합니다. 따로 선택하지 않으면 `20`처럼 메이저 버전을 적은 `.nvmrc` 또는 `.node-version` 파일이 설치된 최신 Node.js 20을 고르고, 그렇지 않으면 기본 버전으로 사이트를 실행합니다. 사이트의 백그라운드 작업과 터미널도 같은 버전을 사용합니다.

## Apache에서 옮겨 오기

OpenLiteSpeed는 기존 Apache 설정을 읽어 `<VirtualHost>`마다 사이트를 하나씩 만들며, 도메인, 문서 루트, SSL 인증서, 리디렉션, 재작성 규칙, 별칭, 프록시, 액세스 규칙, 비밀번호 보호도 함께 옮깁니다. `.htaccess` 파일의 규칙은 OpenLiteSpeed가 직접 적용하므로 계속 동작합니다.

```bat
bin\openlitespeed.exe --import-apache --dry-run
bin\openlitespeed.exe --import-apache C:\Apache24\conf\httpd.conf
bin\openlitespeed.exe -s reload
```

경로를 지정하지 않으면 Laragon, XAMPP, WampServer 또는 `C:\Apache24`의 Apache 설정을 자동으로 찾습니다. `--dry-run`은 아무것도 바꾸지 않고 보고서만 출력합니다. 다시 가져오면 이전에 가져온 내용을 대체하며, `--import-apache --remove`로 제거할 수 있습니다. 변환할 수 없는 규칙은 보고서에 나열됩니다. 액세스 규칙은 그대로 변환되거나 더 엄격하게 변환되며, 더 느슨해지는 일은 없습니다. 유일한 예외는 호스트 이름이나 환경 변수로 특정 클라이언트를 차단하는 규칙이며, 이는 경고로 보고됩니다. 자세한 내용은 [docs/APACHE.md](../APACHE.md)에 있습니다.

## nginx에서 옮겨 오기

`--import-nginx`는 nginx에 대해 같은 일을 합니다: 각 `server` 블록이 도메인, 리스너, SSL 인증서, location, `try_files`, 재작성, 리디렉션, 별칭, 프록시, 액세스 규칙, 비밀번호 보호를 갖춘 사이트가 되며, `fastcgi_pass` 뒤의 PHP는 OpenLiteSpeed의 PHP로 실행됩니다.

```bat
bin\openlitespeed.exe --import-nginx --dry-run
bin\openlitespeed.exe --import-nginx C:\nginx\conf\nginx.conf
```

자세한 내용은 [docs/NGINX.md](../NGINX.md)에 있습니다.

## 여러 CPU 코어

OpenLiteSpeed는 CPU 코어마다 HTTP 워커를 하나씩 시작하며, 기본값은 최대 8개입니다. WebAdmin의 **Server Configuration > General**에 있는 **Number of Workers** 또는 `conf\httpd_config.conf`의 `httpdWorkers`를 1부터 32 사이의 값으로 설정할 수 있습니다. 각 워커는 자체 PHP 풀을 가집니다. Windows에서 워커가 수신 대기 소켓을 어떻게 공유하는지는 [docs/ARCHITECTURE.md](../ARCHITECTURE.md)를 참고하세요.

## 명령줄

명령은 설치 폴더에서 `bin\openlitespeed.exe` 형태로 실행합니다.

| 명령 | 설명 |
|---|---|
| `openlitespeed -s start\|stop\|restart\|reload\|status` | 서버를 제어합니다. `reload`는 다운타임 없는 재시작입니다 |
| `openlitespeed -d` | 포그라운드에서 실행합니다 |
| `openlitespeed --install-service` / `--uninstall-service` | Windows 서비스를 등록하거나 제거합니다 |
| `openlitespeed --set-php <folder>` | 다른 PHP 버전을 사용합니다 |
| `openlitespeed --import-apache [httpd.conf] [--dry-run]` / `--import-apache --remove` | Apache 사이트를 가져옵니다 / 가져온 사이트를 제거합니다 |
| `openlitespeed --import-nginx [nginx.conf] [--dry-run]` / `--import-nginx --remove` | nginx 사이트를 가져옵니다 / 가져온 사이트를 제거합니다 |
| `openlitespeed --acme-install [--email <address>]` | Let's Encrypt 인증서 자동 발급을 켭니다 |
| `openlitespeed --modsec-install [--detection-only]` / `--modsec-install --remove` | OWASP Core Rule Set을 사용하는 ModSecurity 방화벽을 켜거나 끕니다 |
| `openlitespeed --dev-sites <folder> [tld]` | 폴더의 각 하위 폴더마다 사이트를 만듭니다 |
| `openlitespeed --new-site <name> [--wordpress\|--laravel\|--node\|--python]` | 프로젝트 폴더에 사이트를 만듭니다: PHP 사이트, 데이터베이스가 있는 WordPress, Laravel 프로젝트, Node.js 앱 또는 Python 앱 |
| `openlitespeed --site php <site> <folder>\|default` | 한 사이트에 다른 PHP 버전을 사용합니다 |
| `openlitespeed --site app <site> node\|python\|off` | 사이트 폴더를 Node.js 또는 Python 앱으로 실행하거나 다시 PHP 사이트로 되돌립니다 |
| `openlitespeed --site node <site> <node.exe>\|default` | 한 사이트에 다른 Node.js 버전을 사용합니다 |
| `openlitespeed --php list` | 이 컴퓨터의 PHP 버전을 버전별 사이트와 Xdebug 상태와 함께 JSON으로 보여 줍니다 |
| `openlitespeed --php available` / `install <8.x> [ts]` / `remove <folder>` | windows.php.net의 PHP 버전을 나열하고 설치하며, 내려받은 버전을 제거합니다 |
| `openlitespeed --php ext <folder>` / `ext-set <folder> <name> on\|off` | PHP 버전의 확장을 나열하고, 확장을 켜거나 끕니다 |
| `openlitespeed --php ini <folder>` / `ini-set <folder> <key> <value>` | 자주 쓰는 `php.ini` 설정을 읽거나 바꿉니다 |
| `openlitespeed --php xdebug <folder> on\|off\|status` | PHP 버전의 Xdebug를 켜거나 끄거나, 켜져 있는지 보여 줍니다 |
| `openlitespeed --node list\|available` / `install <major>` / `default <node.exe>` / `remove <node.exe>` | 앱에 쓸 Node.js 버전을 관리합니다 |
| `openlitespeed --composer install` | Composer를 `tools\composer`에 내려받습니다 |
| `openlitespeed --set-port http\|admin <port>` | 웹사이트 포트나 WebAdmin 포트를 바꿉니다 |
| `openlitespeed --proxy add <name> <port>` / `remove <name>` / `list` | 로컬 포트의 앱을 위한 프록시 사이트를 추가하거나 제거합니다 |
| `openlitespeed --dev-https on [port]\|off\|status` | 로컬 사이트를 위한 신뢰할 수 있는 HTTPS |
| `openlitespeed --dev-hosts` / `--dev-hosts-clear` | hosts 파일에 사이트 이름을 추가하거나 제거합니다 |
| `openlitespeed --db install\|fetch\|start\|stop\|status` | MariaDB와 phpMyAdmin을 설치, 다운로드만, 시작, 중지합니다 |
| `openlitespeed --db list` / `create <name> [--no-user]` / `drop <name> [--user]` | 데이터베이스를 나열, 생성, 삭제합니다 |
| `openlitespeed --db export <name> <file.sql>` / `import <name> <file.sql>` | 데이터베이스를 파일로 내보내거나 파일을 데이터베이스로 가져옵니다 |
| `openlitespeed --tunnel start <site>` / `stop <site>` / `stop --all` / `status` | Cloudflare Tunnel로 사이트를 공개하거나 공개를 중단합니다 |
| `openlitespeed --tunnel token` / `run` / `run-stop` / `token clear` | 표준 입력에서 터널 토큰을 저장하고, 자체 도메인을 연결하거나 끊고, 토큰을 지웁니다 |
| `openlitespeed --mail on\|off\|status` | 메일 캐처를 켜거나 끄거나, 실행 중인지 보여 줍니다 |
| `openlitespeed --mail list` / `remove <id>` / `save <id> <n> <file>` / `clear` | 수집한 메일을 JSON으로 나열하고, 메시지 하나를 삭제하고, 메시지의 `n`번째(0부터) 첨부 파일을 파일로 저장하거나, 모든 메시지를 삭제합니다 |
| `openlitespeed --mail sendmail [-t] [-i] [-f <from>] [<recipient>...]` | sendmail처럼 표준 입력에서 메시지를 받아 저장합니다 |
| `openlitespeed --job list` / `add <site> <name> <command>` / `remove <site> <name>` | 사이트의 백그라운드 작업을 나열, 추가, 제거합니다 |
| `openlitespeed --job start\|stop\|restart <site> <name>` / `autostart <site> <name> on\|off` | 작업을 제어하고, 웹 서버와 함께 시작할지 정합니다 |
| `openlitespeed --status` | 서버, PHP, 사이트, 데이터베이스, 터널 상태를 JSON으로 보여 줍니다 |
| `openlitespeed --admin-password admin\conf\htpasswd <password>` | WebAdmin 비밀번호를 설정합니다 |

트레이 앱은 서버를 시작, 중지, 다시 로드하고, 사이트, WebAdmin, 로그를 열고, 사이트와 프록시 사이트를 만들고, 신뢰할 수 있는 HTTPS를 켜고 끄고, PHP 버전을 바꾸고, Apache와 nginx 사이트를 가져옵니다. 풀 스택 에디션에서는 대시보드를 열고 MariaDB도 제어합니다. 하루에 한 번 GitHub에서 새 릴리스를 확인하며, **Update**는 새 릴리스를 내려받아 서명과 체크섬을 확인하고 새 설치 프로그램을 실행합니다. `conf\olstray.ini`의 `[update]` 아래에 `check=0`을 넣으면 확인을 끕니다.

## 소스에서 빌드하기

[docs/BUILD.md](../BUILD.md)를 참고하세요. 빌드에는 Visual Studio 2022, CMake, Ninja를 사용하며, 모든 의존성을 고정된 커밋으로 가져옵니다.

## 제한 사항

- 가상 호스트별 사용자 전환(suEXEC)은 없습니다. PHP는 서버를 실행하는 사용자 또는 서비스 계정으로 실행됩니다.
- `SO_REUSEPORT`는 없습니다. 워커는 주소마다 하나의 수신 대기 소켓을 공유합니다.
- cgroups, namespaces, bubblewrap, CageFS는 Linux 기능이므로 사용할 수 없습니다.
- App Server 컨텍스트를 통한 Ruby 애플리케이션은 지원하지 않습니다.

## 기여하기

버그 보고와 풀 리퀘스트를 환영합니다. [CONTRIBUTING.md](../../CONTRIBUTING.md)를 참고하세요. 보안 문제는 [SECURITY.md](../../SECURITY.md)에 설명된 방법으로 비공개로 알려 주세요.

## 라이선스

GPL-3.0이며, [LICENSE](../../LICENSE)를 참고하세요. OpenLiteSpeed는 LiteSpeed Technologies가 개발하며 GPL-3.0 라이선스를 따릅니다. `lsphp.exe`는 PHP 소스로 빌드되며 [PHP License](../../win/lsphp/PHP-LICENSE.txt)가 적용됩니다. 서드파티 구성 요소는 [THIRD-PARTY-NOTICES.txt](../../win/package/THIRD-PARTY-NOTICES.txt)에 나와 있습니다.

이 프로젝트는 LiteSpeed Technologies 또는 PHP Group과 관련이 없으며, 이들의 보증을 받지 않았습니다. LiteSpeed와 OpenLiteSpeed는 LiteSpeed Technologies, Inc.의 상표입니다.

같은 개발자의 다른 프로젝트: [OpenLiteSpeed for cPanel and WHM](https://github.com/NeuralFoundry/openlitespeed-cpanel).
