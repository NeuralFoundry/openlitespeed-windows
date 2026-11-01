param(
    [string[]]$Branches = @('8.2', '8.3', '8.4', '8.5'),
    [string[]]$Kinds = @('ts', 'nts')
)

$ErrorActionPreference = 'Stop'
$W = Split-Path -Parent $MyInvocation.MyCommand.Path
$Top = Split-Path -Parent (Split-Path -Parent $W)
$Deps = Join-Path $Top '.deps\php'
$Build = Join-Path $Top 'build'

$patches = @(
    @{ File = 'lsapidef.h'
       From = '#if defined(__i386__)||defined( __x86_64 )||defined( __x86_64__ )'
       To   = '#if defined(__i386__)||defined( __x86_64 )||defined( __x86_64__ )||defined(_M_IX86)||defined(_M_X64)||defined(_M_ARM64)' },
    @{ File = 'lsapilib.c'
       From = "        if ( n > 1 )`n        {`n            LSAPI_Init_Prefork_Server( n, fp, avoidFork != 0 );`n            LSAPI_Set_Server_fd( g_req.m_fdListen );`n        }"
       To   = "#ifndef _WIN32`n        if ( n > 1 )`n        {`n            LSAPI_Init_Prefork_Server( n, fp, avoidFork != 0 );`n            LSAPI_Set_Server_fd( g_req.m_fdListen );`n        }`n#endif" },
    @{ File = 'lsapi_main.c'
       From = '                strftime( time_buf, 30, "%d/%b/%Y:%H:%M:%S", localtime( &tv_req_end.tv_sec ) );'
       To   = "                time_t t_end = tv_req_end.tv_sec;`n                strftime( time_buf, 30, `"%d/%b/%Y:%H:%M:%S`", localtime( &t_end ) );" },
    @{ File = 'lsapi_main.c'
       From = "        if ( bindFd != 0 ) {`n            dup2( bindFd, 0 );`n            close( bindFd );`n        }"
       To   = "        if ( bindFd != 0 ) {`n#ifdef _WIN32`n            lsw_set_stdin_socket( bindFd );`n#else`n            dup2( bindFd, 0 );`n            close( bindFd );`n#endif`n        }" },
    @{ File = 'lsapilib.c'
       From = "int LSAPI_Finish_r( LSAPI_Request * pReq )`n{`n    /* finish req body */`n    if ( !pReq )`n        return -1;`n    if (pReq->m_reqState)`n    {`n        if ( pReq->m_fd != -1 )`n        {`n"
       To   = "#ifdef _WIN32`nstatic int lsw_last_request(void);`n#endif`n`nint LSAPI_Finish_r( LSAPI_Request * pReq )`n{`n    /* finish req body */`n    if ( !pReq )`n        return -1;`n    if (pReq->m_reqState)`n    {`n        if ( pReq->m_fd != -1 )`n        {`n#ifdef _WIN32`n            if ( lsw_last_request() )`n                send_conn_close_notification( pReq->m_fd );`n#endif`n" },
    @{ File = 'lsapilib.c'
       From = "static unsigned int s_max_reqs = UINT_MAX;`n"
       To   = "static unsigned int s_max_reqs = UINT_MAX;`n#ifdef _WIN32`nstatic int lsw_last_request(void) { return (unsigned int)s_req_processed > s_max_reqs; }`n#endif`n" },
    @{ File = 'lsapilib.c'
       From = "    if ( (unsigned int)s_req_processed > s_max_reqs )`n        return -1;`n`n    if ( s_worker_status )`n    {`n        s_worker_status->m_tmWaitBegin"
       To   = "    if ( (unsigned int)s_req_processed > s_max_reqs )`n    {`n#ifdef _WIN32`n        if ( pReq->m_fd != -1 )`n            lsapi_close_connection( pReq );`n#endif`n        return -1;`n    }`n`n    if ( s_worker_status )`n    {`n        s_worker_status->m_tmWaitBegin" },
    @{ File = 'lsapi_main.c'
       From = "    } else {`n        setArgv0(argc, argv );`n    }"
       To   = "    } else {`n#ifdef _WIN32`n        static char s_title[64];`n        argv0 = s_title;`n#else`n        setArgv0(argc, argv );`n#endif`n    }" },
    @{ File = 'lsapi_main.c'
       From = "    argv0 = sapi_module->executable_location;`n"
       To   = "#ifndef _WIN32`n    argv0 = sapi_module->executable_location;`n#endif`n" },
    @{ File = 'lsapi_main.c'
       From = "    tsrm_shutdown();`n#endif`n    return ret;`n}`n`n/*   LiteSpeed PHP module starts here */"
       To   = "    tsrm_shutdown();`n#endif`n#ifdef _WIN32`n    lsw_exit_now(ret);`n#endif`n    return ret;`n}`n`n/*   LiteSpeed PHP module starts here */" }
)

foreach ($b in $Branches) {
    $src = Join-Path $Deps "$b\litespeed"
    if (-not (Test-Path $src)) { throw "missing sources for PHP $b, run fetch-php.ps1 first" }
    $out = Join-Path $Build "lsphp-src\$b"
    New-Item -ItemType Directory -Force $out | Out-Null
    foreach ($f in Get-ChildItem $src -File) {
        $text = [IO.File]::ReadAllText($f.FullName) -replace "`r`n", "`n"
        foreach ($p in $patches | Where-Object { $_.File -eq $f.Name }) {
            if (-not $text.Contains($p.From)) { throw "PHP ${b}: patch for $($f.Name) does not apply" }
            $text = $text.Replace($p.From, $p.To)
        }
        [IO.File]::WriteAllText((Join-Path $out $f.Name), $text)
    }
    foreach ($k in $Kinds) {
        $devel = Get-ChildItem (Join-Path $Deps "$b\devel-$k") -Directory | Select-Object -First 1
        $lib = Get-ChildItem (Join-Path $devel.FullName 'lib') -Filter 'php8*.lib' | Select-Object -First 1
        if (-not $devel -or -not $lib) { throw "missing devel pack for PHP $b $k" }
        $dest = Join-Path $Build "lsphp-$b-$k"
        cmd /c "`"$W\build.cmd`" `"$out`" `"$($devel.FullName)`" `"$($lib.FullName)`" `"$dest`" > `"$dest.log`" 2>&1"
        if ($LASTEXITCODE -ne 0) { Get-Content "$dest.log" | Select-String 'error' | Select-Object -First 10; throw "build failed: PHP $b $k" }
        Write-Host "lsphp PHP $b $k -> $dest\lsphp.exe"
    }
}
