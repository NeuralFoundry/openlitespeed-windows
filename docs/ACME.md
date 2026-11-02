# Automatic HTTPS certificates

OpenLiteSpeed for Windows gets free certificates from Let's Encrypt, or another ACME certificate authority, for the sites on its HTTPS listeners and renews them before they expire. The ACME client is built into `openlitespeed.exe`; acme.sh, bash and scheduled tasks are not needed.

## Setup

```bat
bin\openlitespeed.exe --acme-install --email you@example.com
bin\openlitespeed.exe -s reload
```

`--acme-install` creates an account with the certificate authority, keeps its key in `conf\cert\acme\account.key`, enables `module mod_acme`, which answers the validation requests, and sets **AutoCert with ACME** to On under **Server Configuration > Tuning > SSL Global Settings**. Without `--email`, the address under `adminEmails` is used. `--server <directory URL>` selects another ACME server, `letsencrypt_test` the Let's Encrypt staging server.

For a certificate to be issued:

- The domain resolves to this server in public DNS.
- Port 80 of this server is reachable from the internet and served by OpenLiteSpeed, because the certificate authority checks `http://<domain>/.well-known/acme-challenge/`.
- The site is mapped on an HTTPS listener, usually port 443. The listener does not need a certificate of its own.

## Which sites get a certificate

| Setting | Result |
|---|---|
| HTTPS listener without certificate files | Every site mapped on it gets a certificate for its domain and aliases |
| HTTPS listener with certificate files | Sites without their own certificate files get one through SNI; the listener keeps its certificate |
| Site with its own certificate files | Never replaced |
| **Virtual Host > SSL > AutoCert with ACME > Enabled** set to Off | No certificate for that site |

With **AutoCert with ACME** set to Off on the server instead of On, only sites where it is enabled under **Virtual Host > SSL** get a certificate.

`www.<domain>` is added when it resolves and answers the validation, and left out otherwise. Domains that do not resolve are skipped and named in the error log.

## Certificates and renewal

Certificates are requested in the background after a start or reload, and OpenLiteSpeed reloads without downtime once they are written. They are checked once a day and renewed when two thirds of their lifetime have passed. The work is logged in `logs\acme.log`.

| File | Content |
|---|---|
| `conf\cert\acme\server.conf` | Directory URL of the ACME server |
| `conf\cert\acme\certs\<domain>_ecc\` | Key, certificate, chain and renewal data of each domain, in the layout acme.sh uses |

When no site uses a certificate any more, its folder is deleted at the next start.

Wildcard certificates need DNS validation, which OpenLiteSpeed for Windows does not support.
