# Importing Apache sites

`openlitespeed --import-apache` reads an Apache 2.4 configuration and creates an OpenLiteSpeed site for every `<VirtualHost>`. Apache and its files are only read, never changed.

## Usage

```bat
bin\openlitespeed.exe --import-apache --dry-run
bin\openlitespeed.exe --import-apache C:\Apache24\conf\httpd.conf
bin\openlitespeed.exe -s reload
```

| Option | Description |
|---|---|
| `[httpd.conf]` | Apache main configuration. When omitted, the configuration of Laragon, XAMPP, WampServer, a running `httpd.exe` or `C:\Apache24` is used. If several are found, they are listed and one has to be passed. |
| `--dry-run` | Print the report, change nothing |
| `--remove` | Remove the sites of the last import |

The report lists every site with its domains, ports and what was converted, followed by notes, warnings and the directives that were not converted, each with file name and line:

```text
Apache configuration: C:/Apache24/conf/httpd.conf (6 files)

shop.example.com  ->  D:/sites/shop/public  [http 80, https 443]
  also: www.shop.example.com
  converted: SSL certificate, 3 rewrite and redirect rules, 2 access rules, password protection, .htaccess files
  note: httpd-vhosts.conf:41  ProxyPassReverse /api/ http://127.0.0.1:3000/  not converted, the backend must send correct redirects

Skipped:
  httpd-vhosts.conf:23  dummy-host.example.com  Apache sample site
```

Stop Apache before reloading OpenLiteSpeed, both cannot listen on the same ports. When OpenLiteSpeed was installed while Apache was running, its Default listener uses port 8088. The report mentions this; change the port to 80 under **Listeners** in WebAdmin afterwards.

## What is converted

| Apache | OpenLiteSpeed |
|---|---|
| `<VirtualHost>` with `ServerName` and `ServerAlias` | Virtual host, domains mapped on the listener of each address and port. Port 80 uses the Default listener, other ports get a new listener on which the first site is the default, as in Apache. |
| `Listen 127.0.0.1:8080` or `<VirtualHost 10.0.0.5:80>` | A listener on that IP address only |
| HTTP and HTTPS blocks with the same `ServerName` | One virtual host when the HTTP block only redirects or both blocks are the same; rules that differ are limited with `RewriteCond %{HTTPS}`. Otherwise two virtual hosts, one per listener. |
| `SSLRequireSSL` | Requests over HTTP to that folder are refused |
| `SSLCertificateFile`, `SSLCertificateKeyFile`, `SSLCertificateChainFile` | `vhssl` of the site and an HTTPS listener per port |
| `DocumentRoot`, `DirectoryIndex`, `Options Indexes` | Document root, index files, directory listing |
| `RewriteEngine`, `RewriteCond`, `RewriteRule`, `RewriteOptions Inherit` | Rewrite rules of the site |
| `Redirect`, `RedirectMatch`, `RedirectPermanent`, `RedirectTemp` | Rewrite rules with `R=` or `G` |
| `Alias`, `ScriptAlias` | Contexts, including the aliases of the main server |
| `ProxyPass` to an `http://` backend, `ProxyPass ... !` | Proxy external app and rewrite rules in the same order |
| `Require`, `RequireAll`, `RequireAny`, `Order`, `Allow`, `Deny`, `Satisfy`, `AuthMerging` | IP access control in contexts. `Require` and `Order`/`Allow`/`Deny` are evaluated separately and both apply, as in Apache. |
| `AuthType Basic`, `AuthName`, `AuthUserFile`, `AuthGroupFile` | Realm with the same password and group files |
| `<Directory>`, `<Files>`, `<Location>` and their `Match` forms | Contexts in Apache's order of precedence. Wildcard and `DirectoryMatch` sections are matched against the folders of the site. File patterns ignore letter case, as Apache does on Windows. |
| `AllowOverride` | `.htaccess` files on or off for the site |
| `Header set`, `append`, `add`, `merge`, `unset` | Extra headers |
| `php_value`, `php_flag`, `php_admin_value`, `php_admin_flag` | PHP settings, applied through LSAPI |
| `php_flag engine off`, `RemoveHandler .php`, `SetHandler none` | PHP files in that folder are blocked |
| `AddType` or `AddHandler` for PHP on other extensions | Script handler for those extensions |
| `SetHandler proxy:fcgi://host:port` | FastCGI external app for PHP |
| `ErrorDocument` with a path or URL | Error page |
| `ErrorLog`, `CustomLog`, `LogLevel`, `LogFormat` | Logs of the site, at the same paths |

`.htaccess` files are not converted, OpenLiteSpeed for Windows reads them itself while it runs: rewrite rules, `Require`, `RequireAll`, `RequireAny`, `Order`, `Allow`, `Deny`, `Satisfy`, Basic authentication, `<Files>` and `<FilesMatch>`, `<Limit>`, `Options Indexes` and `php_flag engine off`. A changed, new or deleted `.htaccess` file takes effect within a second, and its rules apply together with the rules of the site configuration. When `AllowOverride` is enabled for only some folders in Apache, OpenLiteSpeed reads `.htaccess` files in the whole site.

Conditions such as `<IfModule>`, `<IfDefine>`, `<IfVersion>` and `<IfFile>` are evaluated with the modules loaded by the configuration. `Include` and `IncludeOptional` with wildcards, `Define`, `${VAR}` and line continuations are supported. Paths are matched case-insensitively and written with the case they have on disk.

## Stricter, never more permissive

An access rule is either converted exactly or made stricter:

- `RequireAny` or `Satisfy Any` that allows an IP address or a password becomes password only.
- `Require host`, `Require env`, `Require expr` and access rules inside `<If>` deny access.
- Rules inside `<Limit>` and `<LimitExcept>` apply to every request method.
- Two different user or group requirements that must both match deny access.
- In folders where Apache turns PHP off, Apache serves the source of PHP files as a download. OpenLiteSpeed answers 403.
- A folder below a password protected folder stays protected even where Apache opens it again, because contexts inherit the password setting.
- Apache on Windows accepts plain text passwords in password files, OpenLiteSpeed does not. The report names such files.

The one exception are rules that block particular clients by host name or environment variable, such as `Deny from env=bad_bot` or `Require not host`. Turning them into a deny for everyone would take the site offline, so they are left out and listed as warnings.

This Windows build does not run CGI programs. Folders from `ScriptAlias` are imported so that requests there fail and the scripts are never served as text; the report warns about them.

## Not converted

These are listed in the report:

- `AliasMatch`, `ScriptAliasMatch`, `RewriteMap`
- `ProxyPass` to `https://`, `ws://`, `unix:` or `balancer://` backends, and `ProxyPassReverse`
- `Header` with `env=` or `expr=`, `RequestHeader`, `SetEnvIf`, `<If>` without access rules
- `ErrorDocument` with a text message

Sites that require client certificates (`SSLVerifyClient`) are skipped, as are sites without `ServerName`, Apache's sample sites (`www.example.com`, `dummy-host.example.com`) and sites whose domain OpenLiteSpeed already serves, for example a Laragon project that is already in the projects folder. The main Apache server outside `<VirtualHost>` is not imported, OpenLiteSpeed keeps its own default site.

## Files

| File | Content |
|---|---|
| `conf\vhosts\<domain>\vhconf.conf` | Configuration of each imported site, editable in WebAdmin |
| `conf\httpd_config.conf` | `virtualhost` and `listener` entries and domain maps |
| `conf\apache-import.state` | Names of the imported sites and listeners, used to replace or remove them |
| `conf\httpd_config.conf.bak` | The configuration before the last import |

Importing again replaces the previous import. When the new `vhconf.conf` of a site differs from the existing one, the existing file is kept as `vhconf.conf.bak`, so changes made in WebAdmin are not lost. When a certificate chain is a separate file, certificate and chain are combined into `cert-chain.pem` in the site folder; import again after renewing the certificate.
