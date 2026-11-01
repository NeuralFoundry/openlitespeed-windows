# Contributing

Bug reports, fixes and improvements are welcome. Please read this page before opening a pull request.

## Reporting bugs

Open an issue with the bug report form. It asks for your Windows version, how you installed and the relevant lines from `logs\error.log`, which is usually enough to find the cause. Security problems go through private reporting instead, see [SECURITY.md](SECURITY.md).

## Development setup

Build the server as described in [docs/BUILD.md](docs/BUILD.md), then run a development copy in the foreground with `bin\openlitespeed.exe -d`. Use ports other than 80 and 443 if another web server runs on the same machine.

How the port works is described in [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md). Most Windows specific code is in `win\compat\`; changes to the upstream sources are kept small and wrapped in `#ifdef _WIN32`.

## Testing a change

- Serve static files, PHP over LSAPI and over FastCGI, HTTPS, HTTP/2 and HTTP/3.
- Run with `httpdWorkers 1` and with several workers.
- Reload with `openlitespeed -s reload` while a load test is running, and stop the server: no worker may stay behind.
- For installer changes, test a fresh install, an upgrade over an existing install and an uninstall, in standalone, Laragon and XAMPP mode.

## Pull requests

- Keep each pull request to one topic and describe how you tested it.
- Follow the style of the surrounding code. Prefer clear code over comments.
- C code in `win\` must build without warnings in Visual Studio 2022.
- Interface text in the installer and the tray is English: short labels, no explanatory paragraphs.

## License

By contributing you agree that your contribution is licensed under [GPL-3.0](LICENSE).
