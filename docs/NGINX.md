# Importing nginx sites

`openlitespeed --import-nginx` reads an nginx configuration with all its `include` files and creates an OpenLiteSpeed site for every `server` block.

## Usage

```bat
bin\openlitespeed.exe --import-nginx --dry-run
bin\openlitespeed.exe --import-nginx C:\nginx\conf\nginx.conf
bin\openlitespeed.exe -s reload
```

| Option | Description |
|---|---|
| `[nginx.conf]` | nginx main configuration. When omitted, the configuration of Laragon, a running `nginx.exe`, a `nginx*` folder at the root of a drive or WinNMP is used. If several are found, they are listed and one has to be passed. |
| `--dry-run` | Print the report, change nothing |
| `--remove` | Remove the sites of the last nginx import |

The tray app has the same function under **Sites > Import nginx sites**. Importing again replaces the earlier nginx import. Sites imported from Apache are kept apart and are not touched.

## What is converted

| nginx | OpenLiteSpeed |
|---|---|
| `server` with `listen` and `server_name`, including `*.example.com` and `.example.com` | Virtual host with its domains, mapped on a listener for each address and port |
| HTTP and HTTPS `server` blocks with the same `server_name` | One virtual host when the HTTP block only redirects or both blocks are the same, otherwise two |
| `ssl_certificate`, `ssl_certificate_key` | `vhssl` of the site and an HTTPS listener |
| `root`, `index`, `autoindex` | Document root, index files, directory listing |
| `location` with no modifier, `=`, `^~`, `~` and `~*` | Contexts and rules in the order nginx selects locations |
| `alias`, `root` in a location | Context with that location, including `$1` in regex locations |
| `allow`, `deny` | IP access control, first match as in nginx |
| `auth_basic`, `auth_basic_user_file` | Realm with the same password file |
| `add_header` | Extra headers |
| `return` with a redirect, `403` or `410` | Rewrite rules |
| `rewrite` with `last`, `break`, `redirect`, `permanent` | Rewrite rules |
| `try_files $uri $uri/ <fallback>` | Rewrite rules that send missing files to the fallback, for example `/index.php` |
| `if ($scheme = http)`, `if ($host = ...)`, `if (!-e $request_filename)` | Rewrite conditions |
| `proxy_pass` to an `http://` backend or an `upstream` | Proxy external app and rewrite rule |
| `fastcgi_pass` | PHP runs through the PHP of OpenLiteSpeed |
| `error_page` with a path | Error page |

The variables `$host`, `$http_host`, `$server_name`, `$request_uri`, `$uri`, `$args` and `$query_string` are converted. Rules that use other variables are listed in the report.

## Not converted

Directives that cannot be converted are listed in the report with their file and line, for example `set`, `map`, `limit_req`, `if` with other conditions, regular expression server names, `proxy_pass` to `https://` and `error_page` with `=code` or a named location. Nested locations are converted as locations of their own. `client_max_body_size` is set for the whole server in OpenLiteSpeed.

## Files

| File | Content |
|---|---|
| `conf\vhosts\<domain>\vhconf.conf` | Configuration of each imported site, editable in WebAdmin |
| `conf\httpd_config.conf` | `virtualhost` and `listener` entries and domain maps |
| `conf\nginx-import.state` | Names of the imported sites and listeners, used to replace or remove them |
