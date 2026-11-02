# Modules

On Linux, OpenLiteSpeed loads modules from `.so` files in the `modules` folder. OpenLiteSpeed for Windows has them built into `openlitespeed.exe`.

| Module | Loaded |
|---|---|
| `cache` (LSCache), `modcompress`, `moddecompress` | Always |
| `mod_acme` | When automatic certificates are set up, see [ACME.md](ACME.md) |
| `uploadprogress`, `modreqparser` | When `conf\httpd_config.conf` has a `module` block for them, as on Linux |

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
