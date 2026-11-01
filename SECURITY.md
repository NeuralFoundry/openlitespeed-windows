# Security policy

OpenLiteSpeed for Windows serves websites and can run as a Windows service, so security reports are taken seriously.

## Supported versions

Security fixes are made for the latest release and the `main` branch.

## Reporting a vulnerability

Do not open a public issue. Report the problem privately through GitHub:
[Report a vulnerability](https://github.com/NeuralFoundry/openlitespeed-windows/security/advisories/new)

Please include the version, the Windows version, the steps to reproduce and what an attacker can achieve. Reports are acknowledged within 7 days, and you will be credited in the advisory unless you prefer otherwise.

## Scope

In scope: the Windows compatibility layer in `win\`, the Windows specific changes to the OpenLiteSpeed sources, `lsphp.exe` for Windows, the installer, the tray application and the default configuration they write.

Problems that also affect OpenLiteSpeed on Linux should be reported to [LiteSpeed Technologies](https://openlitespeed.org), and problems in PHP itself to the [PHP project](https://www.php.net/security).
