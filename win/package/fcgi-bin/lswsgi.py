import importlib.util
import os
import sys


def load(root, startup):
    path = startup if os.path.isabs(startup) else os.path.join(root, startup)
    spec = importlib.util.spec_from_file_location('wsgi_app', path)
    module = importlib.util.module_from_spec(spec)
    sys.modules['wsgi_app'] = module
    spec.loader.exec_module(module)
    app = getattr(module, 'application', None) or getattr(module, 'app', None)
    if app is None:
        raise SystemExit('%s has no application' % path)
    return app


def main():
    root = os.path.abspath(os.environ.get('WSGI_ROOT') or os.getcwd())
    host, _, port = (os.environ.get('OLS_APP_ADDRESS') or '').rpartition(':')
    prefix = (os.environ.get('OLS_APP_URI') or '/').rstrip('/')
    log = os.environ.get('LS_STDERR_LOG')
    if log:
        try:
            log = os.path.abspath(log)
            os.makedirs(os.path.dirname(log), exist_ok=True)
            sys.stderr = open(log, 'a', buffering=1, encoding='utf-8', errors='replace')
        except OSError:
            pass
    if not host or not port.isdigit():
        raise SystemExit('OLS_APP_ADDRESS is not set')
    os.chdir(root)
    if root not in sys.path:
        sys.path.insert(0, root)
    app = load(root, os.environ.get('LSWSGI_STARTUP') or 'wsgi.py')

    def mounted(environ, start_response):
        p = environ.get('PATH_INFO', '')
        if prefix and (p == prefix or p.startswith(prefix + '/')):
            environ['SCRIPT_NAME'] = prefix
            environ['PATH_INFO'] = p[len(prefix):]
        return app(environ, start_response)

    threads = max(4, min(32, int(os.environ.get('LSAPI_CHILDREN') or 8)))
    try:
        import waitress
    except ImportError:
        waitress = None
    if waitress:
        waitress.serve(mounted, host=host, port=int(port), threads=threads, ident=None)
        return
    from socketserver import ThreadingMixIn
    from wsgiref.simple_server import WSGIRequestHandler, WSGIServer, make_server

    class Server(ThreadingMixIn, WSGIServer):
        daemon_threads = True

    class Handler(WSGIRequestHandler):
        def log_message(self, *args):
            pass

    make_server(host, int(port), mounted, Server, Handler).serve_forever()


if __name__ == '__main__':
    main()
