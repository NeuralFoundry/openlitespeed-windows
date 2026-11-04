# Node.js and Python applications

OpenLiteSpeed starts Node.js and Python (WSGI) applications itself and passes requests to them. Static files in the `public` folder of the application are served directly.

## Apps in the projects folder

The simplest way is a site in the projects folder. In the dashboard, **New site** with **Node.js app** or **Python app** creates `name.test` with a small starter app, and **More > Run as a Node.js app** turns an existing folder with a `package.json` into an app. From the command line:

```bat
bin\openlitespeed.exe --new-site chat --node
bin\openlitespeed.exe --site app reports python
bin\openlitespeed.exe -s reload
```

The whole site is passed to the app, and files in its `public` folder are served directly. Node.js apps run with the default Node.js of the dashboard (`openlitespeed --node default <node.exe>`), Python apps with the Python found on the computer. `--site app <name> off` makes the folder a PHP site again.

## Setup

For an app under a path of another site, add a context of type **App Server** in WebAdmin, or add it to `vhconf.conf`:

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
| `startupFile` | Optional. Without it the start script or `main` in `package.json` is used, or a usual name such as `server.js` | `wsgi.py` by default, it has to define `application` or `app` |
| `binPath` | Path to `node.exe`, found on `PATH`, in Laragon or in `Program Files` when empty | Path to `python.exe`, found on `PATH`, in Laragon or in the usual Python folders when empty |
| `appserverEnv` | `NODE_ENV`: 0 development, 1 production, 2 staging | `WSGI_ENV` |

Node.js applications run through [openlitespeed-node](https://github.com/NeuralFoundry/openlitespeed-node) and need no changes: it runs CommonJS, ES modules and TypeScript, starts Next.js, serves apps that only export an Express app or a fetch handler, and moves `app.listen(3000)` to the address of the app. Requests carry the real client address, so `req.ip` and `req.secure` work. WebSockets work without extra settings.

With `npm install -g openlitespeed-node`, `ols-node doctor <folder>` shows how an app will start and `ols-node add <folder> --uri /app/` adds the context. An app that has `openlitespeed-node` in its `node_modules` is started with that version of the runner.

Python applications run with [waitress](https://pypi.org/project/waitress/) when it is installed (`pip install waitress`), otherwise with the HTTP server of the Python standard library. Requests under the context path reach the WSGI application with that path as `SCRIPT_NAME`.

Console output and errors go to `log\stderr.log` in the application folder. When an application process ends, it is started again with the next request, and it is stopped together with the server. Writing `tmp\restart.txt` in the application folder restarts it within 10 seconds.

## How it works on Windows

On Linux, OpenLiteSpeed talks to applications over Unix domain sockets. Node.js and Python on Windows cannot listen on those, so every application listens on a fixed port on `127.0.0.1` between 30000 and 61999, chosen from the site and context name, with one port per HTTP worker. The starters are `fcgi-bin\lsnode.js` and `fcgi-bin\lswsgi.py`.

Ruby applications (`appType rails`) are not supported on Windows.
