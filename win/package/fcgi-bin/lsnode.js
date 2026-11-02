'use strict';

const fs = require('fs');
const net = require('net');
const path = require('path');
const url = require('url');

const root = path.resolve(process.env.LSNODE_ROOT || process.cwd());
const startup = process.env.LSNODE_STARTUP_FILE || 'app.js';
const address = /^(.*):(\d+)$/.exec(process.env.OLS_APP_ADDRESS || '');

if (process.env.LS_STDERR_LOG) {
    try {
        const file = path.resolve(process.env.LS_STDERR_LOG);
        fs.mkdirSync(path.dirname(file), { recursive: true });
        const log = fs.createWriteStream(file, { flags: 'a' });
        log.on('error', () => {});
        process.stderr.write = log.write.bind(log);
    } catch (e) {
    }
}
if (!address) {
    console.error('OLS_APP_ADDRESS is not set');
    process.exit(1);
}

const listen = net.Server.prototype.listen;
let bound = false;
net.Server.prototype.listen = function (...args) {
    if (bound)
        return listen.apply(this, args);
    bound = true;
    const cb = args.find((a) => typeof a === 'function');
    return listen.call(this, { host: address[1], port: Number(address[2]) }, cb);
};

process.chdir(root);
const file = path.resolve(root, startup);
if (file.endsWith('.mjs')) {
    import(url.pathToFileURL(file).href).catch((e) => {
        console.error(e);
        process.exit(1);
    });
} else {
    require(file);
}
