# How OpenLiteSpeed runs on Windows

This port builds OpenLiteSpeed 1.9.4 as a native Win32 program with Microsoft Visual C++. There is no Cygwin, MSYS2 or WSL layer. Almost all of the server code is the upstream code; the Windows specific work lives in `win/`, and the upstream sources carry small `#ifdef _WIN32` changes where Windows behaves differently.

## Compatibility layer

`win/compat` implements the POSIX interfaces OpenLiteSpeed uses on top of Win32. Every source file is compiled with `win/compat/include/ols_win.h` forced in, so the server code keeps calling `open`, `read`, `socket`, `epoll_wait`, `fork`-free process functions and so on.

| Area | Implementation |
|---|---|
| File descriptors | A descriptor is the Win32 `HANDLE` or `SOCKET` shifted right by two, so sockets and files share one number space without a lookup table. Per descriptor flags (non-blocking, append, close-on-exec) are kept in a small side table. |
| Event loop | `epoll` is provided by [wepoll](https://github.com/piscisaureus/wepoll), which talks to the AFD driver directly. It is level triggered, like the upstream epoll multiplexer. |
| Signals | `kill()` sets a named event `Local\ols-sig-<pid>-<signal>` with a private ACL. A thread in the target process raises the signal and wakes the event loop through a socket pair, so `SIGTERM`, `SIGHUP` and `SIGUSR1` keep their meaning. `SIGCHLD` is raised when a child process exits. |
| Paths | The server works with drive-less absolute paths such as `/Users/me/www`; they are resolved against the drive the server runs from. Every file open is checked against the real on-disk name to block Windows specific tricks: wrong letter case, trailing dots and spaces, `::$DATA` streams, 8.3 short names and device names such as `CON` and `NUL`. A name that cannot be verified is refused. |
| Shared memory | `mmap` on a file maps the same file view in every process, so LSCache, the SSL session cache, TLS ticket keys and the DNS cache are shared between HTTP workers. Locks in shared memory are atomic spin locks and work across processes. |
| Unix sockets | `AF_UNIX` sockets are native Windows Unix domain sockets. Socket files get an ACL that only allows SYSTEM, Administrators and the server account. |
| Socket pairs | Windows has no `socketpair()`. A stream pair is a native `AF_UNIX` connection set up in a temporary folder that only SYSTEM, Administrators and the server account can open, and removed right after. TCP over loopback is the fallback. The LSAPI connection to every `lsphp.exe` is such a pair, and `AF_UNIX` makes PHP requests 5 to 8 percent faster than loopback TCP. |

## Processes

```
openlitespeed.exe            main process, HTTP worker #1, WebAdmin, HTTP/3
 ├─ openlitespeed.exe        HTTP worker #2 .. #N
 ├─ lsphp.exe / php-cgi.exe  PHP pool of worker #1
 └─ ...                      each worker has its own PHP pool
```

### HTTP workers

On Linux, OpenLiteSpeed forks one process per CPU and gives each one its own `SO_REUSEPORT` socket. Windows has neither `fork` nor `SO_REUSEPORT`, so:

- The main process creates the listening sockets, loads the configuration and starts serving.
- It then starts `httpdWorkers - 1` copies of itself. Each copy is created suspended, receives the TCP listening sockets through `WSADuplicateSocket` and a private hand-off file, loads the configuration itself and starts serving the same ports.
- Workers are placed in a job object that is closed when the main process exits, so no worker can outlive it. The main process restarts a worker that crashes, at most ten times per minute.
- WebAdmin, the admin socket and the UDP sockets for HTTP/3 stay in the main process, matching the listener binding rules upstream uses for multiple workers. Workers still send the `Alt-Svc` header, so browsers discover HTTP/3 whichever worker answers.

When several processes call `accept()` on one shared socket, Windows can leave the process that loses the race blocked inside a non-blocking `accept()` until the next connection arrives. A blocked worker cannot serve its open connections or react to a stop request. Shared listening sockets are therefore protected by a cross-process accept mutex: one process accepts at a time, the others skip the round and are woken again by the level-triggered event loop. The mutex name contains a random key that is passed only in the private hand-off file.

`httpdWorkers` defaults to the number of CPUs, up to 8, and can be set from 1 to 32.

### Zero downtime reload

`openlitespeed -s reload` (or **Graceful Restart** in WebAdmin) starts a new main process and hands it every listening socket, including UDP. The new main process starts its own workers. When it is ready it tells the old main process, which stops its workers; old processes finish the requests they are serving and exit. Clients see no refused connections.

### PHP

PHP runs in separate processes, so PHP work always uses every CPU, whatever the number of HTTP workers.

- **LSAPI**: `lsphp.exe` is the PHP LiteSpeed SAPI built for Windows from the PHP sources, for PHP 8.2, 8.3, 8.4 and 8.5, thread safe and non thread safe. OpenLiteSpeed starts an `lsphp` process for each connection it needs and talks to it over a socket pair. `lsphp` recycles itself after `LSAPI_MAX_REQS` requests and tells the server before it does, so no request is sent to a process that is exiting.
- **FastCGI**: `php-cgi.exe` from any PHP build, listening on a Unix socket per worker.
- `openlitespeed --set-php <dir>` detects the PHP version and build type, copies the matching `lsphp.exe` into the PHP directory, starts it once with `-v` to prove it loads, and falls back to FastCGI if it does not.
- PHP processes run with a restricted token that holds no privileges, inside a job object owned by their worker.

### Windows service

`openlitespeed --install-service` registers the `OpenLiteSpeed` service. The service process is a small supervisor that runs the server and restarts it after a crash. It runs as the virtual account `NT SERVICE\OpenLiteSpeed` with only `SeChangeNotifyPrivilege`. Installing the service sets ACLs on the install folder: the account can read the program, and write only to the configuration, log, temporary and cache folders. Uninstalling the service restores the inherited ACLs.

## Fixes made on the way

These are bugs in upstream OpenLiteSpeed that the Windows port exposed. They are fixed in this tree:

- `HttpSession` left `m_pAioReq` uninitialized on platforms without Linux AIO, which crashed HTTP/3 sessions under load.
- A request could be retried on a new external application connection after part of its response had already been sent to the client, so the client received the body twice. `HttpExtConnector::isRecoverable()` now refuses a retry once response data has arrived.
- The WebAdmin listener served plain HTTP when its certificate was missing and ACME was enabled.

## Limitations

- No per virtual host user switching (suEXEC). PHP runs as the server account, or as the service account in service mode.
- No `SO_REUSEPORT`; HTTP workers share one listening socket per address.
- At most 32 HTTP workers, because listener bindings are stored as 32 bit masks on Windows.
- cgroups, namespaces, bubblewrap and CageFS are Linux features and are not available.
