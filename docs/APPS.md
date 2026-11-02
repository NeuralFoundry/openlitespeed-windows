# Node.js and Python applications

OpenLiteSpeed starts Node.js and Python (WSGI) applications itself and passes requests to them. Static files in the `public` folder of the application are served directly.

## Setup

In WebAdmin, add a context of type **App Server** to the site, or add it to `vhconf.conf`:

```text
context /app/ {
  type                     appserver
  location                 C:/sites/myapp/
  appType                  node
  startupFile              app.js
}
```

| Setting | Node.js | Python |
|---|---|---|
| `appType` | `node` | `wsgi` |
| `startupFile` | `app.js` by default, `.mjs` files are loaded as ES modules | `wsgi.py` by default, it has to define `application` or `app` |
| `binPath` | Path to `node.exe`, found on `PATH`, in Laragon or in `Program Files` when empty | Path to `python.exe`, found on `PATH`, in Laragon or in the usual Python folders when empty |
| `appserverEnv` | `NODE_ENV`: 0 development, 1 production, 2 staging | `WSGI_ENV` |

A Node.js application starts its server as usual, for example `app.listen(3000)`; OpenLiteSpeed gives it its own address instead. Python applications run with [waitress](https://pypi.org/project/waitress/) when it is installed (`pip install waitress`), otherwise with the HTTP server of the Python standard library. Requests under the context path reach the WSGI application with that path as `SCRIPT_NAME`.

Errors written to the console go to `log\stderr.log` in the application folder. When an application process ends, it is started again with the next request, and it is stopped together with the server.

## How it works on Windows

On Linux, OpenLiteSpeed talks to applications over Unix domain sockets. Node.js and Python on Windows cannot listen on those, so every application listens on a fixed port on `127.0.0.1` between 30000 and 61999, chosen from the site and context name, with one port per HTTP worker. The starters are `fcgi-bin\lsnode.js` and `fcgi-bin\lswsgi.py`.

Ruby applications (`appType rails`) are not supported on Windows.
