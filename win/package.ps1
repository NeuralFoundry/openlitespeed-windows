param([string]$Out)

$ErrorActionPreference = 'Stop'
$Top = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$Dist = Join-Path $Top 'dist'
$Pkg = Join-Path $Top 'win\package'
$Version = (Get-Content (Join-Path $Dist 'VERSION') -Raw).Trim()
if (-not $Out) { $Out = Join-Path $Top "build\OpenLiteSpeed-$Version-win64" }

if (Test-Path $Out) { Remove-Item -Recurse -Force $Out }
foreach ($d in 'bin', 'conf\vhosts\Example', 'conf\cert', 'admin\conf', 'admin\tmp', 'admin\logs',
               'Example\logs', 'logs', 'tmp', 'share', 'lsrecaptcha') {
    New-Item -ItemType Directory -Force (Join-Path $Out $d) | Out-Null
}

Copy-Item (Join-Path $Top 'build\openlitespeed.exe') (Join-Path $Out 'bin')
Copy-Item (Join-Path $Top 'build\olstray.exe') (Join-Path $Out 'bin')
Copy-Item (Join-Path $Top 'build\lsrecaptcha.exe') (Join-Path $Out 'bin')
Copy-Item (Join-Path $Dist 'lsrecaptcha\_recaptcha.shtml') (Join-Path $Out 'lsrecaptcha')
Get-ChildItem (Join-Path $Top 'build') -Directory -Filter 'lsphp-*' | Where-Object { $_.Name -match '^lsphp-\d+\.\d+-n?ts$' } | ForEach-Object {
    $exe = Join-Path $_.FullName 'lsphp.exe'
    if (Test-Path $exe) {
        $dest = Join-Path $Out ('bin\lsphp\' + $_.Name.Substring(6))
        New-Item -ItemType Directory -Force $dest | Out-Null
        Copy-Item $exe $dest
    }
}
Copy-Item (Join-Path $Pkg 'httpd_config.conf') (Join-Path $Out 'conf')
Copy-Item (Join-Path $Dist 'conf\mime.properties') (Join-Path $Out 'conf')
Copy-Item (Join-Path $Pkg 'vhconf.conf') (Join-Path $Out 'conf\vhosts\Example')
Copy-Item (Join-Path $Pkg 'admin_config.conf') (Join-Path $Out 'admin\conf')
Copy-Item (Join-Path $Pkg 'php.ini') (Join-Path $Out 'admin\conf\php.ini.in')
Copy-Item -Recurse (Join-Path $Dist 'admin\html.open') (Join-Path $Out 'admin\html')
Copy-Item -Recurse (Join-Path $Dist 'admin\html.disabled') (Join-Path $Out 'admin\html.disabled')
Copy-Item -Recurse (Join-Path $Dist 'Example\html') (Join-Path $Out 'Example\html')
Copy-Item -Recurse (Join-Path $Dist 'share\autoindex') (Join-Path $Out 'share\autoindex')
$Msc = Join-Path $Top '.deps\modsecurity'
$Crs = Join-Path $Top '.deps\coreruleset'
New-Item -ItemType Directory -Force (Join-Path $Out 'conf\modsec'), (Join-Path $Out 'modsec\crs\rules') | Out-Null
Get-ChildItem (Join-Path $Crs 'rules') -File | Where-Object { $_.Name -notin 'RESPONSE-955-WEB-SHELLS.conf', 'web-shells-php.data' } |
    Copy-Item -Destination (Join-Path $Out 'modsec\crs\rules')
Copy-Item (Join-Path $Crs 'LICENSE') (Join-Path $Out 'modsec\crs\LICENSE')
Copy-Item (Join-Path $Crs 'crs-setup.conf.example') (Join-Path $Out 'conf\modsec\crs-setup.conf')
Copy-Item (Join-Path $Msc 'unicode.mapping') (Join-Path $Out 'conf\modsec')
$rec = [IO.File]::ReadAllText((Join-Path $Msc 'modsecurity.conf-recommended')).Replace('SecRuleEngine DetectionOnly', 'SecRuleEngine On')
[IO.File]::WriteAllText((Join-Path $Out 'conf\modsec\modsecurity.conf'), $rec)
[IO.File]::WriteAllText((Join-Path $Out 'conf\modsec\main.conf'),
    "Include modsecurity.conf`nInclude crs-setup.conf`nInclude ../../modsec/crs/rules/*.conf`n")
Copy-Item -Recurse (Join-Path $Pkg 'fcgi-bin') (Join-Path $Out 'fcgi-bin')
Copy-Item (Join-Path $Pkg 'install.ps1') $Out
Copy-Item (Join-Path $Top 'LICENSE') (Join-Path $Out 'LICENSE.txt')
Copy-Item (Join-Path $Pkg 'THIRD-PARTY-NOTICES.txt') $Out
if (Test-Path (Join-Path $Out 'bin\lsphp')) {
    Copy-Item (Join-Path $Top 'win\lsphp\PHP-LICENSE.txt') (Join-Path $Out 'bin\lsphp\PHP-LICENSE.txt')
}

$setup = "$Out-setup.exe"
& (Join-Path $Top 'build\olspack.exe') (Join-Path $Top 'build\setup.exe') $Out $setup
if ($LASTEXITCODE -ne 0) { throw 'building setup.exe failed' }

$zip = "$Out.zip"
if (Test-Path $zip) { Remove-Item -Force $zip }
Add-Type -AssemblyName System.IO.Compression, System.IO.Compression.FileSystem
$base = (Split-Path -Parent $Out).Length + 1
$zs = [IO.File]::Open($zip, [IO.FileMode]::CreateNew)
$za = New-Object IO.Compression.ZipArchive($zs, [IO.Compression.ZipArchiveMode]::Create)
foreach ($item in @(Get-Item -LiteralPath $Out) + @(Get-ChildItem -LiteralPath $Out -Recurse)) {
    $name = $item.FullName.Substring($base).Replace('\', '/')
    if ($item.PSIsContainer) {
        if (-not (Get-ChildItem -LiteralPath $item.FullName -Force | Select-Object -First 1)) { [void]$za.CreateEntry("$name/") }
    } else {
        [void][IO.Compression.ZipFileExtensions]::CreateEntryFromFile($za, $item.FullName, $name, [IO.Compression.CompressionLevel]::Optimal)
    }
}
$za.Dispose()
$zs.Dispose()
Write-Host "package: $Out"
Write-Host "zip:     $zip ($([int]((Get-Item $zip).Length / 1MB)) MB)"
