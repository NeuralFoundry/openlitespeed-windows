param(
    [string[]]$Branches = @('8.2', '8.3', '8.4', '8.5'),
    [switch]$WithBinaries
)

$ErrorActionPreference = 'Stop'
$Top = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path))
$Deps = Join-Path $Top '.deps\php'
$Base = 'https://windows.php.net/downloads/releases'
New-Item -ItemType Directory -Force $Deps | Out-Null

function Get-File([string]$url, [string]$dest, [string]$sha) {
    if ((Test-Path $dest) -and $sha -and ((Get-FileHash $dest -Algorithm SHA256).Hash -eq $sha.ToUpper())) { return }
    curl.exe -sfL -o $dest $url
    if ($LASTEXITCODE -ne 0) { throw "download failed: $url" }
    if ($sha -and ((Get-FileHash $dest -Algorithm SHA256).Hash -ne $sha.ToUpper())) {
        Remove-Item $dest
        throw "checksum mismatch: $url"
    }
}

curl.exe -sfL -o (Join-Path $Deps 'releases.json') "$Base/releases.json"
if ($LASTEXITCODE -ne 0) { throw 'cannot download releases.json' }
$rel = Get-Content (Join-Path $Deps 'releases.json') -Raw | ConvertFrom-Json

foreach ($b in $Branches) {
    $info = $rel.$b
    if (-not $info) { throw "unknown PHP branch $b" }
    $ver = $info.version
    $tag = 'php-' + ($ver -replace '-\d+$', '')
    $dir = Join-Path $Deps $b
    New-Item -ItemType Directory -Force $dir | Out-Null
    Set-Content (Join-Path $dir 'VERSION') $ver -Encoding ascii
    foreach ($kind in 'ts', 'nts') {
        $key = $info.PSObject.Properties.Name | Where-Object { $_ -like "$kind-*-x64" } | Select-Object -First 1
        $entry = $info.$key
        $devel = Join-Path $dir "devel-$kind.zip"
        Get-File "$Base/$($entry.devel_pack.path)" $devel $entry.devel_pack.sha256
        $out = Join-Path $dir "devel-$kind"
        if (Test-Path $out) { Remove-Item -Recurse -Force $out }
        Expand-Archive -LiteralPath $devel -DestinationPath $out -Force
        if ($WithBinaries) {
            $zip = Join-Path $dir "php-$kind.zip"
            Get-File "$Base/$($entry.zip.path)" $zip $entry.zip.sha256
            $bin = Join-Path $dir "php-$kind"
            if (-not (Test-Path (Join-Path $bin 'php-cgi.exe'))) {
                Expand-Archive -LiteralPath $zip -DestinationPath $bin -Force
            }
        }
    }
    $src = Join-Path $dir 'litespeed'
    New-Item -ItemType Directory -Force $src | Out-Null
    foreach ($f in 'lsapi_main.c', 'lsapilib.c', 'lsapilib.h', 'lsapidef.h', 'lsapi_main_arginfo.h') {
        curl.exe -sfL -o (Join-Path $src $f) "https://raw.githubusercontent.com/php/php-src/$tag/sapi/litespeed/$f"
        if ($LASTEXITCODE -ne 0) { throw "cannot download $f for $tag" }
    }
    Write-Host "PHP $ver ($tag) ready"
}
