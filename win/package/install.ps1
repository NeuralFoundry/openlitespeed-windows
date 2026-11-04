param(
    [string]$PhpPath,
    [string]$DownloadPhp,
    [string]$AdminPassword,
    [int]$HttpPort = 0,
    [int]$AdminPort = 7080,
    [string]$Www,
    [string]$Tld = 'test',
    [ValidateSet('auto', 'standalone', 'laragon', 'xampp')]
    [string]$Mode = 'auto',
    [ValidateSet('server', 'full')]
    [string]$Edition = 'server',
    [switch]$Service,
    [switch]$Tray,
    [switch]$PhpOnly,
    [switch]$Quiet
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$Exe = Join-Path $Root 'bin\openlitespeed.exe'
$env:OLS_ROOT = $Root
foreach ($old in 'dbg-8.4-nts', 'exp-8.4-nts') {
    $d = Join-Path $Root "bin\lsphp\$old"
    if (Test-Path -LiteralPath $d) { Remove-Item -LiteralPath $d -Recurse -Force }
}

function Fwd([string]$p) { return $p.Replace('\', '/') }
function Say([string]$m) { if (-not $Quiet) { Write-Host $m } }

function Get-PhpInfo([string]$dir, [string]$source) {
    $cgi = Join-Path $dir 'php-cgi.exe'
    if (-not (Test-Path -LiteralPath $cgi)) { return $null }
    try {
        $fs = [IO.File]::OpenRead($cgi)
        $br = New-Object IO.BinaryReader($fs)
        $fs.Position = 0x3C
        $fs.Position = $br.ReadInt32() + 4
        $machine = $br.ReadUInt16()
        $fs.Close()
    } catch { return $null }
    if ($machine -ne 0x8664) { return $null }
    $vi = (Get-Item -LiteralPath $cgi).VersionInfo
    if ($vi.FileMajorPart -lt 7) { return $null }
    $ts = Test-Path (Join-Path $dir "php$($vi.FileMajorPart)ts.dll")
    $kind = if ($ts) { 'ts' } else { 'nts' }
    $branch = "$($vi.FileMajorPart).$($vi.FileMinorPart)"
    return [pscustomobject]@{
        Dir = (Resolve-Path -LiteralPath $dir).Path.TrimEnd('\')
        Source = $source
        Version = [version]"$branch.$($vi.FileBuildPart)"
        Ts = $ts
        Lsapi = Test-Path (Join-Path $Root "bin\lsphp\$branch-$kind\lsphp.exe")
    }
}

function Find-AllPhp {
    $list = @()
    $add = { param($d, $s) $i = Get-PhpInfo $d $s; if ($i) { $script:found += $i } }
    $script:found = @()
    function Scan([string]$base, [string]$pattern, [string]$source) {
        if (-not (Test-Path -LiteralPath $base -PathType Container)) { return }
        & $add $base $source
        Get-ChildItem -LiteralPath $base -Directory -Filter $pattern -ErrorAction SilentlyContinue |
            ForEach-Object { & $add $_.FullName $source }
    }
    if ($LaragonDir) { Scan "$LaragonDir\bin\php" '*' 'Laragon' }
    if ($XamppDir) { & $add "$XamppDir\php" 'XAMPP' }
    Scan (Join-Path $Root 'php') '*' 'OpenLiteSpeed'
    foreach ($dir in ($env:PATH -split ';')) { if ($dir) { & $add $dir 'PATH' } }
    foreach ($drive in [IO.DriveInfo]::GetDrives() | Where-Object { $_.DriveType -eq 'Fixed' -and $_.IsReady }) {
        $r = $drive.RootDirectory.FullName
        Get-ChildItem $r -Directory -Filter 'php*' -ErrorAction SilentlyContinue | ForEach-Object { Scan $_.FullName '*' 'PHP' }
        Get-ChildItem $r -Directory -Filter 'wamp*' -ErrorAction SilentlyContinue |
            ForEach-Object { Scan (Join-Path $_.FullName 'bin\php') '*' 'WampServer' }
        if (Test-Path "$r\tools") {
            Get-ChildItem "$r\tools" -Directory -Filter 'php*' -ErrorAction SilentlyContinue |
                ForEach-Object { Scan $_.FullName '*' 'Chocolatey' }
        }
    }
    Scan "$env:ProgramFiles\PHP" '*' 'PHP'
    Get-ChildItem "$env:USERPROFILE\scoop\apps" -Directory -Filter 'php*' -ErrorAction SilentlyContinue |
        ForEach-Object { & $add (Join-Path $_.FullName 'current') 'Scoop' }
    Scan "$env:LOCALAPPDATA\Microsoft\WinGet\Packages" 'PHP.PHP*' 'WinGet'
    $seen = @{}
    foreach ($i in $script:found) {
        if (-not $seen.ContainsKey($i.Dir.ToLower())) { $seen[$i.Dir.ToLower()] = 1; $list += $i }
    }
    return $list | Sort-Object Version -Descending
}

function Find-Php {
    $all = @(Find-AllPhp)
    foreach ($i in $all) { Say "Found PHP $($i.Version) $(if ($i.Ts) { 'TS' } else { 'NTS' })$(if ($i.Lsapi) { ' LSAPI' })  [$($i.Source)]  $($i.Dir)" }
    $best = $all | Where-Object { $_.Lsapi } | Select-Object -First 1
    if (-not $best) { $best = $all | Select-Object -First 1 }
    if ($best) { return $best.Dir }
    return $null
}

function Find-Laragon {
    $p = Get-Process laragon -ErrorAction SilentlyContinue | Where-Object { $_.Path } | Select-Object -First 1
    if ($p) { return Split-Path $p.Path }
    foreach ($k in 'HKLM:\Software\Microsoft\Windows\CurrentVersion\Uninstall\*',
                   'HKLM:\Software\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\*',
                   'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\*') {
        $e = Get-ItemProperty $k -ErrorAction SilentlyContinue |
            Where-Object { $_.DisplayName -like 'Laragon*' -and $_.InstallLocation } | Select-Object -First 1
        if ($e -and (Test-Path (Join-Path $e.InstallLocation 'laragon.exe'))) { return $e.InstallLocation.TrimEnd('\') }
    }
    foreach ($drive in [IO.DriveInfo]::GetDrives() | Where-Object { $_.DriveType -eq 'Fixed' -and $_.IsReady }) {
        $d = Join-Path $drive.RootDirectory.FullName 'laragon'
        if (Test-Path (Join-Path $d 'laragon.exe')) { return $d }
    }
    return $null
}

function Find-Xampp {
    foreach ($k in 'HKLM:\SOFTWARE\xampp', 'HKLM:\SOFTWARE\WOW6432Node\xampp') {
        $d = (Get-ItemProperty $k -ErrorAction SilentlyContinue).Install_Dir
        if ($d -and (Test-Path (Join-Path $d 'php\php-cgi.exe'))) { return $d.TrimEnd('\') }
    }
    $p = Get-Process xampp-control -ErrorAction SilentlyContinue | Where-Object { $_.Path } | Select-Object -First 1
    if ($p -and (Test-Path (Join-Path (Split-Path $p.Path) 'php\php-cgi.exe'))) { return Split-Path $p.Path }
    foreach ($drive in [IO.DriveInfo]::GetDrives() | Where-Object { $_.DriveType -eq 'Fixed' -and $_.IsReady }) {
        $d = Join-Path $drive.RootDirectory.FullName 'xampp'
        if (Test-Path (Join-Path $d 'php\php-cgi.exe')) { return $d }
    }
    return $null
}

function Enable-Extensions([string]$dir) {
    $ini = Join-Path $dir 'php.ini'
    if (Test-Path $ini) { return }
    $src = Join-Path $dir 'php.ini-production'
    if (-not (Test-Path $src)) { $src = Join-Path $dir 'php.ini-development' }
    if (-not (Test-Path $src)) { return }
    $text = Get-Content $src -Raw
    $text = $text -replace ';\s*extension_dir = "ext"', ('extension_dir = "' + (Fwd (Join-Path $dir 'ext')) + '"')
    foreach ($e in 'bz2', 'curl', 'fileinfo', 'gd', 'gettext', 'intl', 'mbstring', 'exif', 'mysqli', 'openssl',
                   'pdo_mysql', 'pdo_sqlite', 'sqlite3', 'sodium', 'zip') {
        if (Test-Path (Join-Path $dir "ext\php_$e.dll")) { $text = $text -replace "(?m)^;extension=$e\s*$", "extension=$e" }
    }
    $text = $text -replace '(?m)^upload_max_filesize = .*$', 'upload_max_filesize = 64M'
    $text = $text -replace '(?m)^post_max_size = .*$', 'post_max_size = 64M'
    $text = $text -replace '(?m)^memory_limit = .*$', 'memory_limit = 256M'
    Set-Content $ini $text -Encoding ascii
}

function New-PrivateTemp {
    $base = if ($isAdmin) { Join-Path $env:SystemRoot 'Temp' } else { $env:TEMP }
    $dir = Join-Path $base ('ols-' + [guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory $dir | Out-Null
    if ($isAdmin) {
        $acl = New-Object Security.AccessControl.DirectorySecurity
        $acl.SetAccessRuleProtection($true, $false)
        foreach ($sid in 'S-1-5-32-544', 'S-1-5-18') {
            $acl.AddAccessRule((New-Object Security.AccessControl.FileSystemAccessRule(
                (New-Object Security.Principal.SecurityIdentifier $sid), 'FullControl', 'ContainerInherit,ObjectInherit', 'None', 'Allow')))
        }
        Set-Acl -LiteralPath $dir -AclObject $acl
    }
    return $dir
}

function Install-Php([string]$branch) {
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    $base = 'https://windows.php.net/downloads/releases'
    $rel = Invoke-RestMethod "$base/releases.json" -UseBasicParsing
    $info = $rel.$branch
    if (-not $info) { throw "PHP $branch is not available from windows.php.net" }
    $key = $info.PSObject.Properties.Name | Where-Object { $_ -like 'nts-*-x64' } | Select-Object -First 1
    $entry = $info.$key.zip
    $dest = Join-Path $Root "php\$($info.version)"
    if (-not (Test-Path (Join-Path $dest 'php-cgi.exe'))) {
        $tmp = New-PrivateTemp
        $zip = Join-Path $tmp 'php.zip'
        Say "Downloading PHP $($info.version)..."
        & (Join-Path $env:SystemRoot 'System32\curl.exe') -sfL --proto =https -o $zip "$base/$($entry.path)"
        if ($LASTEXITCODE -ne 0) { Invoke-WebRequest "$base/$($entry.path)" -OutFile $zip -UseBasicParsing }
        $fs = [IO.File]::Open($zip, 'Open', 'Read', 'Read')
        try {
            $hash = -join ([Security.Cryptography.SHA256]::Create().ComputeHash($fs) | ForEach-Object { $_.ToString('X2') })
            if ($hash -ne $entry.sha256.ToUpper()) { throw 'downloaded PHP failed the checksum check' }
            New-Item -ItemType Directory -Force $dest | Out-Null
            Expand-Archive -LiteralPath $zip -DestinationPath $dest -Force
        } finally {
            $fs.Dispose()
            Remove-Item -LiteralPath $tmp -Recurse -Force -ErrorAction SilentlyContinue
        }
    }
    Enable-Extensions $dest
    return $dest
}

function Test-PhpStarts([string]$php) {
    $o = [IO.Path]::GetTempFileName()
    $e = [IO.Path]::GetTempFileName()
    $p = Start-Process (Join-Path $php 'php-cgi.exe') -ArgumentList '-v' -PassThru -Wait -WindowStyle Hidden `
        -RedirectStandardOutput $o -RedirectStandardError $e
    $text = "$(Get-Content $o -Raw)$(Get-Content $e -Raw)".Trim()
    Remove-Item $o, $e -Force
    return @{ Code = $p.ExitCode; Text = $text }
}

function Ensure-VcRuntime([string]$php) {
    $r = Test-PhpStarts $php
    if ($r.Code -eq 0) { return }
    $vcMissing = $r.Code -eq -1073741515 -and -not (Test-Path "$env:SystemRoot\System32\vcruntime140.dll")
    if (-not $vcMissing -and $r.Text -notmatch 'is not compatible with this PHP build') {
        throw "php-cgi.exe in $php does not start (exit code $($r.Code)): $($r.Text)"
    }
    if (-not $isAdmin) { throw 'PHP needs the Microsoft Visual C++ runtime; run the installer as Administrator' }
    Say 'Installing the Microsoft Visual C++ runtime...'
    $tmp = New-PrivateTemp
    $vc = Join-Path $tmp 'vc_redist.x64.exe'
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    Invoke-WebRequest 'https://aka.ms/vs/17/release/vc_redist.x64.exe' -OutFile $vc -UseBasicParsing
    $fs = [IO.File]::Open($vc, 'Open', 'Read', 'Read')
    try {
        $sig = Get-AuthenticodeSignature $vc
        if ($sig.Status -ne 'Valid' -or $sig.SignerCertificate.Subject -notlike '*Microsoft Corporation*') {
            throw 'the Visual C++ runtime download is not signed by Microsoft'
        }
        Start-Process $vc -ArgumentList '/install', '/quiet', '/norestart' -Wait
    } finally {
        $fs.Dispose()
        Remove-Item -LiteralPath $tmp -Recurse -Force -ErrorAction SilentlyContinue
    }
    $r = Test-PhpStarts $php
    if ($r.Code -ne 0) { throw "php-cgi.exe in $php still does not start: $($r.Text)" }
}

function Ini-Value([string]$file, [string]$section, [string]$key) {
    if (-not (Test-Path $file)) { return $null }
    $cur = ''
    foreach ($line in Get-Content $file) {
        if ($line -match '^\s*\[(.+)\]\s*$') { $cur = $Matches[1]; continue }
        if ($cur -eq $section -and $line -match "^\s*$key\s*=\s*(.*)$") { return $Matches[1].Trim() }
    }
    return $null
}

function Port-Free([int]$port) {
    try {
        $l = [Net.Sockets.TcpListener]::new([Net.IPAddress]::Any, $port)
        $l.Start(); $l.Stop()
        return $true
    } catch { return $false }
}

$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
    [Security.Principal.WindowsBuiltInRole]::Administrator)

$up2 = Split-Path (Split-Path $Root)
$up1 = Split-Path $Root
$LaragonDir = if ($up2 -and (Test-Path (Join-Path $up2 'laragon.exe'))) { $up2 } else { Find-Laragon }
$XamppDir = if ($up1 -and (Test-Path (Join-Path $up1 'php\php-cgi.exe')) -and (Test-Path (Join-Path $up1 'htdocs'))) { $up1 } else { Find-Xampp }

if ($Mode -eq 'auto') {
    if ($LaragonDir -and $up2 -eq $LaragonDir) { $Mode = 'laragon' }
    elseif ($XamppDir -and $up1 -eq $XamppDir) { $Mode = 'xampp' }
    elseif ($LaragonDir) { $Mode = 'laragon' }
    elseif ($XamppDir) { $Mode = 'xampp' }
    else { $Mode = 'standalone' }
}
if ($Mode -eq 'laragon' -and -not $LaragonDir) { throw 'Laragon was not found' }
if ($Mode -eq 'xampp' -and -not $XamppDir) { throw 'XAMPP was not found' }
if ($LaragonDir) { Say "Laragon:  $LaragonDir" }
if ($XamppDir) { Say "XAMPP:    $XamppDir" }

$laragonIni = Join-Path $LaragonDir 'usr\laragon.ini'
if ($Mode -eq 'laragon') {
    if (-not $PhpPath -and -not $DownloadPhp) {
        $v = Ini-Value $laragonIni 'php' 'Version'
        if ($v -and (Test-Path "$LaragonDir\bin\php\$v\php-cgi.exe")) { $PhpPath = "$LaragonDir\bin\php\$v" }
    }
    if (-not $Www) {
        $Www = Ini-Value $laragonIni 'apache' 'DocumentRoot'
        if (-not $Www) { $Www = Join-Path $LaragonDir 'www' }
    }
} elseif ($Mode -eq 'xampp') {
    if (-not $PhpPath -and -not $DownloadPhp -and (Test-Path "$XamppDir\php\php-cgi.exe")) { $PhpPath = "$XamppDir\php" }
    if (-not $Www -and (Test-Path "$XamppDir\htdocs")) { $Www = "$XamppDir\htdocs" }
}
if (-not $Www) { $Www = Join-Path $Root 'www' }

$phpNote = $null
if ($DownloadPhp) {
    try { $PhpPath = Install-Php $DownloadPhp }
    catch { $phpNote = "PHP $DownloadPhp could not be downloaded: $($_.Exception.Message)"; $PhpPath = $null }
}
if (-not $PhpPath) { $PhpPath = Find-Php }
if (-not $PhpPath -and -not $phpNote) {
    try { $PhpPath = Install-Php '8.4' }
    catch { $phpNote = "PHP 8.4 could not be downloaded: $($_.Exception.Message)" }
}
if ($PhpPath) {
    if (-not (Test-Path (Join-Path $PhpPath 'php-cgi.exe'))) { throw "php-cgi.exe not found in $PhpPath" }
    $PhpPath = (Resolve-Path $PhpPath).Path
    try { Ensure-VcRuntime $PhpPath }
    catch { $phpNote = $_.Exception.Message; $PhpPath = $null }
}
if ($PhpOnly -and -not $PhpPath) { throw $phpNote }

function Set-NoPhp {
    $conf = Join-Path $Root 'conf\httpd_config.conf'
    $block = "# php begin`r`nextProcessor lsphp {`r`n    type                         lsapi`r`n    address                      uds://tmp/lshttpd/nophp.sock`r`n" +
        "    maxConns                     1`r`n    retryTimeout                 0`r`n    autoStart                    0`r`n}`r`n`r`n" +
        "scriptHandler {`r`n    add                          lsapi:lsphp php`r`n}`r`n# php end"
    $text = Get-Content $conf -Raw
    $text = [regex]::Replace($text, '(?s)# php begin.*?# php end', { param($m) $block })
    Set-Content $conf $text -Encoding ascii -NoNewline
}

function Set-Php {
    if (-not $PhpPath) {
        Set-NoPhp
        return
    }
    & $Exe --set-php $PhpPath
    if ($LASTEXITCODE -ne 0) { throw 'configuring PHP failed' }
    $adminPhp = Join-Path $Root 'admin\php'
    $hadAdminPhp = Test-Path $adminPhp
    if ($hadAdminPhp) { cmd /c rmdir "$adminPhp" | Out-Null }
    cmd /c mklink /J "$adminPhp" "$PhpPath" | Out-Null
    $enable = Join-Path $Root 'admin\conf\enable_webadmin'
    if (-not $hadAdminPhp -and -not (Test-Path $enable)) { Set-Content $enable '' -Encoding ascii }
    $ini = Join-Path $Root 'admin\conf\php.ini'
    (Get-Content "$ini.in" -Raw).Replace('%ROOT%', (Fwd $Root)).Replace('%PHP_EXT%', (Fwd (Join-Path $PhpPath 'ext'))) |
        Set-Content $ini -Encoding ascii -NoNewline
}

if ($PhpOnly) {
    Set-Php
    Say "PHP:      $PhpPath"
    exit 0
}

if ($HttpPort -eq 0) { $HttpPort = if (Port-Free 80) { 80 } else { 8088 } }

foreach ($d in 'logs', 'tmp', 'conf\cert', 'admin\tmp', 'admin\logs', 'Example\logs') {
    New-Item -ItemType Directory -Force (Join-Path $Root $d) | Out-Null
}
New-Item -ItemType Directory -Force $Www | Out-Null

$conf = Join-Path $Root 'conf\httpd_config.conf'
(Get-Content $conf -Raw).Replace('%HTTP_PORT%', "$HttpPort") | Set-Content $conf -Encoding ascii -NoNewline
Set-Php

$adminConf = Join-Path $Root 'admin\conf\admin_config.conf'
(Get-Content $adminConf -Raw).Replace('%ADMIN_PORT%', "$AdminPort") | Set-Content $adminConf -Encoding ascii -NoNewline

$key = Join-Path $Root 'admin\conf\webadmin.key'
if (-not (Test-Path $key)) {
    & $Exe --gen-cert $key (Join-Path $Root 'admin\conf\webadmin.crt') $env:COMPUTERNAME
    if ($LASTEXITCODE -ne 0) { throw 'certificate generation failed' }
}

$generated = $false
if (-not $AdminPassword) {
    $chars = 'ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz23456789'
    $bytes = New-Object byte[] 16
    [Security.Cryptography.RandomNumberGenerator]::Create().GetBytes($bytes)
    $AdminPassword = -join ($bytes | ForEach-Object { $chars[$_ % $chars.Length] })
    $generated = $true
}
$prevEncoding = $OutputEncoding
$OutputEncoding = New-Object System.Text.UTF8Encoding $false
$AdminPassword | & $Exe --admin-password (Join-Path $Root 'admin\conf\htpasswd') '-' admin
$adminRc = $LASTEXITCODE
$OutputEncoding = $prevEncoding
if ($adminRc -ne 0) { throw 'setting the admin password failed' }

$withRoot = $Mode -ne 'standalone'
$trayIni = Join-Path $Root 'conf\olstray.ini'
Set-Content $trayIni "[sites]`r`nwww=$Www`r`ntld=$Tld`r`nroot=$([int]$withRoot)`r`nstack=$Mode`r`n[tray]`r`nedition=$Edition`r`n" -Encoding unicode
if ($withRoot) { & $Exe --dev-sites $Www $Tld --root | Out-Null } else { & $Exe --dev-sites $Www $Tld | Out-Null }
$hostsOk = $false
if ($isAdmin) {
    & $Exe --dev-hosts | Out-Null
    $hostsOk = ($LASTEXITCODE -eq 0)
}
if ($Edition -eq 'full') {
    Say 'Downloading MariaDB and phpMyAdmin...'
    if ($isAdmin) { & $Exe --db fetch } else { & $Exe --db install }
    if ($LASTEXITCODE -ne 0) { Say 'MariaDB was not installed. Install it later from the dashboard.' }
    Say 'Downloading Cloudflare Tunnel...'
    & $Exe --tunnel install | Out-Null
    if ($LASTEXITCODE -ne 0) { Say 'Cloudflare Tunnel was not downloaded. It is downloaded again when you share a site.' }
    if ($isAdmin) {
        & $Exe --dev-hosts | Out-Null
        $hostsOk = ($LASTEXITCODE -eq 0)
    }
}

if ($Mode -eq 'laragon') {
    $procfile = Join-Path $LaragonDir 'usr\Procfile'
    $line = "OpenLiteSpeed: autorun `"$Exe`" -d"
    if ((Test-Path $procfile) -and -not (Select-String -Path $procfile -SimpleMatch 'OpenLiteSpeed:' -Quiet)) {
        Add-Content $procfile $line -Encoding ascii
    }
}

if ($Service) {
    & $Exe --install-service
    if ($LASTEXITCODE -ne 0) { throw 'service installation failed (run as Administrator)' }
    Start-Service OpenLiteSpeed
    Say 'Service OpenLiteSpeed installed and started.'
} elseif ($Tray) {
    $run = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run'
    Set-ItemProperty $run 'OpenLiteSpeed' "`"$(Join-Path $Root 'bin\olstray.exe')`""
    if ($isAdmin) { Start-Process (Join-Path $env:SystemRoot 'explorer.exe') -ArgumentList "`"$(Join-Path $Root 'bin\olstray.exe')`"" }
    else { Start-Process (Join-Path $Root 'bin\olstray.exe') -ArgumentList '--start' }
    Say 'Tray app started (OpenLiteSpeed icon next to the clock).'
} else {
    Say "Start:  `"$Exe`" -s start"
    Say "Stop:   `"$Exe`" -s stop"
}

$portPart = if ($HttpPort -eq 80) { '' } else { ":$HttpPort" }
Say ''
Say "Mode:     $Mode"
if ($PhpPath) { Say "PHP:      $PhpPath" }
else { Say "PHP:      not installed. $phpNote. Run the installer again to download PHP. WebAdmin starts once PHP is installed." }
Say "Website:  http://localhost$portPart/"
Say "Projects: $Www  (each folder -> http://<folder>.$Tld$portPart/)"
if (-not $hostsOk) { Say "          hosts file not updated: use the tray menu Sites > Refresh sites (asks for admin rights)" }
if ($PhpPath) { Say "WebAdmin: https://localhost:$AdminPort/  user: admin" }
if ($Edition -eq 'full') { Say "Dashboard: $(Join-Path $Root 'bin\olspanel.exe')" }
if ($generated) { Say "Admin password: $AdminPassword  (change it with: bin\openlitespeed.exe --admin-password admin\conf\htpasswd NEWPASS)" }
