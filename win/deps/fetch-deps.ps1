$ErrorActionPreference = 'Stop'
$Top = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path))
$Deps = Join-Path $Top '.deps'
New-Item -ItemType Directory -Force $Deps | Out-Null

$pins = @(
    @{ Dir = 'boringssl'; Url = 'https://github.com/google/boringssl.git';          Commit = '8b43ff0f72137b31b23d482f0b2344186d797b03'; Patch = 'boringssl.patch' },
    @{ Dir = 'brotli';    Url = 'https://github.com/google/brotli.git';             Commit = 'db24a98aed38a16de78cc05ca15cdd8dc2a0d92d' },
    @{ Dir = 'libbcrypt'; Url = 'https://github.com/litespeedtech/libbcrypt.git';   Commit = '55ff64349dec3012cfbbb1c4f92d4dbd46920213' },
    @{ Dir = 'libexpat';  Url = 'https://github.com/libexpat/libexpat.git';         Commit = 'a76b1174c1ea52abb639a4c69d52b630a8a106a2' },
    @{ Dir = 'pcre2';     Url = 'https://github.com/PCRE2Project/pcre2.git';        Commit = '2dce7761b1831fd3f82a9c2bd5476259d945da4d' },
    @{ Dir = 'udns';      Url = 'https://github.com/ortclib/udns.git';              Commit = 'e0148dfba90ce0d17a8e2cae15f218b5ab99ad62' },
    @{ Dir = 'libmaxminddb'; Url = 'https://github.com/maxmind/libmaxminddb.git';   Commit = '4a3d17a621c0ebcb81498fd34d313f67fd940ab5' },
    @{ Dir = 'ip2location';  Url = 'https://github.com/chrislim2888/IP2Location-C-Library.git'; Commit = 'e0ecd2216fb8d373fa1b3a687051100379c9f600' },
    @{ Dir = 'luajit';       Url = 'https://github.com/LuaJIT/LuaJIT.git';          Commit = 'ea9eb7edb5eac4a9d175d74523db34fac0fa5180' },
    @{ Dir = 'modsecurity';  Url = 'https://github.com/owasp-modsecurity/ModSecurity.git'; Commit = '1925753989ccce977cdaae417b55c9726c7cf02c' },
    @{ Dir = 'yajl';         Url = 'https://github.com/lloyd/yajl.git';             Commit = 'a0ecdde0c042b9256170f2f8890dd9451a4240aa' },
    @{ Dir = 'libxml2';      Url = 'https://gitlab.gnome.org/GNOME/libxml2.git';    Commit = '96498992efa48d52b0e8b83058bd88dbdaf153c1' },
    @{ Dir = 'coreruleset';  Url = 'https://github.com/coreruleset/coreruleset.git'; Commit = 'e03a4f6dabc7a30ebd8c52c97d28a154f590a48f' },
    @{ Dir = 'zlib';      Url = 'https://github.com/madler/zlib.git';               Commit = 'da607da739fa6047df13e66a2af6b8bec7c2a498' }
)

foreach ($p in $pins) {
    $dir = Join-Path $Deps $p.Dir
    if (-not (Test-Path (Join-Path $dir '.git'))) {
        git init -q $dir
        git -C $dir remote add origin $p.Url
    }
    git -C $dir config core.longpaths true
    git -C $dir config core.autocrlf false
    $head = git -C $dir rev-parse -q --verify HEAD 2>$null
    if ($head -ne $p.Commit) {
        Write-Host "fetching $($p.Dir) $($p.Commit.Substring(0, 12))"
        git -C $dir fetch -q --depth 1 origin $p.Commit
        if ($LASTEXITCODE -ne 0) { throw "fetch failed: $($p.Dir)" }
        git -C $dir -c advice.detachedHead=false checkout -q -f FETCH_HEAD
        if ($LASTEXITCODE -ne 0) { throw "checkout failed: $($p.Dir)" }
        if ($p.Patch) {
            git -C $dir apply (Join-Path $PSScriptRoot "patches\$($p.Patch)")
            if ($LASTEXITCODE -ne 0) { throw "patch failed: $($p.Patch)" }
        }
    }
    if (Test-Path (Join-Path $dir '.gitmodules')) {
        git -C $dir submodule update -q --init --recursive
        if ($LASTEXITCODE -ne 0) { throw "submodules failed: $($p.Dir)" }
    }
}
Write-Host "dependency sources ready in $Deps"
