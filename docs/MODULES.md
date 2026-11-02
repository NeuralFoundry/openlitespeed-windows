# Modules

On Linux, OpenLiteSpeed loads modules from `.so` files in the `modules` folder. OpenLiteSpeed for Windows has them built into `openlitespeed.exe`.

| Module | Loaded |
|---|---|
| `cache` (LSCache), `modcompress`, `moddecompress` | Always |
| `mod_acme` | When automatic certificates are set up, see [ACME.md](ACME.md) |
| `mod_security` | When the firewall is turned on, see [PROTECTION.md](PROTECTION.md#web-application-firewall) |
| `uploadprogress`, `modreqparser`, `mod_lua` | When `conf\httpd_config.conf` has a `module` block for them, as on Linux |

## uploadprogress

Reports how much of an upload has arrived. Add the module and a context that answers the progress requests:

```text
module uploadprogress {
  ls_enabled               1
}
```

```text
context /progress {
  type                     module
  handler                  uploadprogress
}
```

Start the upload with `?X-Progress-ID=<id>` in its URL and poll `/progress?X-Progress-ID=<id>` from the page. The answer is JSON, for example `{ "state" : "uploading", "size" : 1000314, "received" : 368640 }`, then `{ "state" : "done" }`.

## modreqparser

Parses request bodies in the server. With `uploadPassByPath 1`, uploaded files are written to `uploadTmpDir` while they arrive, and the application receives the form fields `<field>_name`, `<field>_path`, `<field>_content-type`, `<field>_size` and `<field>_md5` instead of the file data. On Windows, `<field>_path` is a full Windows path. The file is deleted when the request ends, so the application has to move or copy it.

```text
module modreqparser {
  param <<<END_param
uploadTmpDir /tmp/lshttpd/
uploadTmpFilePermission 600
uploadPassByPath 1
END_param
  ls_enabled               1
}
```

With `uploadPassByPath 0`, PHP receives uploads in `$_FILES` as usual.

## mod_lua

Runs Lua scripts with LuaJIT 2.1, which is built into `openlitespeed.exe`. Add the module and a context whose requests are answered by scripts:

```text
module mod_lua {
  ls_enabled               1
  param <<<END_param
maxruntime 5000
maxlinecount 10000000
END_param
}
```

```text
context /lua/ {
  type                     module
  handler                  mod_lua
}
```

A request for `/lua/hello.lua` runs `hello.lua` in the `lua` folder of the document root:

```lua
ls.header["X-Powered-By"] = "Lua"
local args = ls.req.get_uri_args()
ls.say("Hello ", args["name"] or "world")
```

Scripts can read the request and its body, set headers and the status, use shared dictionaries (`ls.shared.<name>`), regular expressions (`ls.re`), timers (`ls.sleep`) and outbound TCP connections (`ls.socket.tcp`) without blocking the server.

| Setting | Meaning |
|---|---|
| `maxruntime` | Milliseconds a script may run before it is stopped |
| `maxlinecount` | Lua instructions after which a running script pauses so that other requests are served. Setting it turns the JIT compiler off, because compiled loops are never interrupted. |

Without both settings, a script that loops forever keeps an HTTP worker busy until the server is stopped. Scripts are loaded from source; precompiled LuaJIT bytecode is not supported.
