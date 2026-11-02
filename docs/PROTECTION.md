# Bot and abuse protection

OpenLiteSpeed for Windows has the same protection as OpenLiteSpeed on Linux: reCAPTCHA or hCaptcha challenges for suspicious visitors and limits per client IP address. Both are set under **Server Configuration > Security** in WebAdmin, or in `conf\httpd_config.conf`.

## reCAPTCHA and hCaptcha

```text
lsrecaptcha {
  enabled                  1
  type                     1
  siteKey                  <site key>
  secretKey                <secret key>
  maxTries                 3
  regConnLimit             15000
  sslConnLimit             10000
  allowedRobotHits         3
  botWhiteList             <user agent pattern, one per line>
}
```

| Setting | Meaning |
|---|---|
| `type` | 1 checkbox, 2 invisible, 3 hCaptcha |
| `siteKey`, `secretKey` | Keys from [Google reCAPTCHA](https://www.google.com/recaptcha/admin) or hCaptcha. Without keys, the built-in LiteSpeed keys are used. |
| `regConnLimit`, `sslConnLimit` | Above this many HTTP or HTTPS connections, every new visitor is challenged |
| `maxTries` | Challenges a visitor may fail. After that the connection limit check refuses the visitor; a rewrite rule does so with `E=verifycaptcha:deny`. |
| `allowedRobotHits` | Requests that known crawlers such as Googlebot, `curl` and `wget`, and the `botWhiteList` user agents, may make without a challenge |

A visitor can also be challenged by a rewrite rule, for example only on a login page:

```apache
RewriteCond %{REQUEST_URI} ^/wp-login\.php
RewriteRule .* - [E=verifycaptcha]
```

A virtual host can enable it and use its own keys with an `lsrecaptcha` block in its configuration. A visitor who passes the challenge is not challenged again. Visitors from the server's own address are never challenged. While a visitor has an open challenge, its other requests to protected pages wait instead of being served.

The challenge page is `lsrecaptcha\_recaptcha.shtml`. Put a `_recaptcha_custom.shtml` next to it to use your own design. The answer is checked by `bin\lsrecaptcha.exe`, which needs outbound HTTPS to `www.google.com` or `api.hcaptcha.com`.

## Limits per client

```text
perClientConnLimit {
  staticReqPerSec          40
  dynReqPerSec             4
  outBandwidth             0
  inBandwidth              0
  softLimit                20
  hardLimit                40
  gracePeriod              15
  banPeriod                60
}
```

| Setting | Meaning |
|---|---|
| `staticReqPerSec`, `dynReqPerSec` | Requests per second for static files and for PHP or other scripts. Requests above the limit wait for the next second, they are not refused. 0 means no limit. |
| `outBandwidth`, `inBandwidth` | Bytes per second per client. 0 means no limit. |
| `softLimit`, `gracePeriod` | A client with more connections than `softLimit` loses keep-alive. If it stays above the limit for `gracePeriod` seconds, it is blocked. |
| `hardLimit` | Connections above this number are closed at once and the client is blocked |
| `banPeriod` | Seconds a blocked client is refused |

IP addresses that are trusted in **Server Configuration > Security > Access Control** (an `allow` entry ending in `T`) are not limited.

Measured on Windows with `staticReqPerSec 5`, `dynReqPerSec 2`, `outBandwidth 102400`, `softLimit 3`, `hardLimit 5`, `gracePeriod 3`, `banPeriod 10`:

| Test | Result |
|---|---|
| 15 static requests on one connection | 2.0 s, served 5 per second |
| 8 PHP requests on one connection | 3.3 s, served 2 per second |
| 600 KB download | 103 KB/s |
| 7 connections at once | 5 served, the rest closed, client blocked |
| 4 connections held open for 5 s | client blocked after the grace period |
| Blocked client after 10 s | served again |

## Country of the visitor

OpenLiteSpeed looks up each client address in a MaxMind GeoIP2 or IP2Location database and passes the result to PHP and to rewrite rules. The databases are not included. GeoLite2 is free with a [MaxMind account](https://www.maxmind.com/en/geolite2/signup), IP2Location LITE with an [IP2Location account](https://lite.ip2location.com).

```text
enableIpGeo              1
geoipDB C:/OpenLiteSpeed/geoip/GeoLite2-Country.mmdb {
  geoipDBName            COUNTRY_DB
}
ip2locDB {
  ip2locDBFile           C:/OpenLiteSpeed/geoip/IP2LOCATION-LITE-DB1.BIN
  ip2locDBCache          MemoryCache
}
```

| Database | Variables |
|---|---|
| `GeoLite2-Country.mmdb` | `GEOIP_COUNTRY_CODE`, `GEOIP_COUNTRY_NAME`, `GEOIP_CONTINENT_CODE` |
| `GeoLite2-City.mmdb` | Also `GEOIP_REGION`, `GEOIP_CITY`, `GEOIP_POSTAL_CODE`, `GEOIP_LATITUDE`, `GEOIP_LONGITUDE` |
| `GeoLite2-ASN.mmdb` | `GEOIP_ORGANIZATION`, `GEOIP_ISP` |
| IP2Location | `IP2LOCATION_COUNTRY_SHORT`, `IP2LOCATION_COUNTRY_LONG` and the other fields of the database |

Several `geoipDB` blocks can be loaded at the same time. A rewrite rule can then block countries, for example:

```apache
RewriteCond %{ENV:GEOIP_COUNTRY_CODE} ^(CN|RU)$
RewriteRule .* - [F]
```

`ip2locDBCache` is `FileIo`, `MemoryCache` or `SharedMemoryCache`. On Windows `SharedMemoryCache` works like `MemoryCache`, so each HTTP worker keeps its own copy of the database. A database that cannot be read is tested in a separate process and disabled with an error in the log; the server keeps running.

## Several HTTP workers

Every HTTP worker keeps its own list of clients, as on Linux. Request and bandwidth limits apply to each connection as configured, but connection counts and passed challenges are counted per worker: with 8 workers a client can open up to 8 times `hardLimit` connections, and a visitor may be challenged again when a new connection reaches another worker. Set **Number of Workers** to 1 when exact connection limits matter more than using every CPU core.
