param([string]$Out)

$ErrorActionPreference = 'Stop'
$Top = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$Dist = Join-Path $Top 'dist'
$Pkg = Join-Path $Top 'win\package'
$Version = (Get-Content (Join-Path $Dist 'VERSION') -Raw).Trim()
if (-not $Out) { $Out = Join-Path $Top "build\OpenLiteSpeed-$Version-win64" }

if (Test-Path $Out) { Remove-Item -Recurse -Force $Out }
foreach ($d in 'bin', 'conf\vhosts\Example', 'conf\cert', 'admin\conf', 'admin\tmp', 'admin\logs',
               'Example\logs', 'logs', 'tmp', 'share') {
    New-Item -ItemType Directory -Force (Join-Path $Out $d) | Out-Null
}

Copy-Item (Join-Path $Top 'build\openlitespeed.exe') (Join-Path $Out 'bin')
Copy-Item (Join-Path $Top 'build\olstray.exe') (Join-Path $Out 'bin')
Get-ChildItem (Join-Path $Top 'build') -Directory -Filter 'lsphp-*' | ForEach-Object {
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
Compress-Archive -Path $Out -DestinationPath $zip
Write-Host "package: $Out"
Write-Host "zip:     $zip ($([int]((Get-Item $zip).Length / 1MB)) MB)"
