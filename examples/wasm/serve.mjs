// examples/wasm/serve.mjs
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// A minimal static file server for index.html:  node serve.mjs [port]
// The module uses threads (-pthread), i.e. SharedArrayBuffer, which browsers
// only provide to cross-origin isolated pages: hence the two headers below.

import { createServer } from 'node:http';
import { readFile } from 'node:fs/promises';
import { extname, join, normalize } from 'node:path';
import { fileURLToPath } from 'node:url';

const root = fileURLToPath(new URL('.', import.meta.url));
const port = Number(process.argv[2] ?? 8080);
const types = {
    '.html': 'text/html',
    '.mjs': 'text/javascript',
    '.js': 'text/javascript',
    '.wasm': 'application/wasm',
    '.ts': 'text/plain',
};

createServer(async (request, response) => {
    const path = new URL(request.url ?? '/', 'http://localhost').pathname;
    const file = join(root, normalize(path === '/' ? '/index.html' : path));
    response.setHeader('Cross-Origin-Opener-Policy', 'same-origin');
    response.setHeader('Cross-Origin-Embedder-Policy', 'require-corp');
    try {
        if (!file.startsWith(root)) throw new Error('outside of the root');
        const body = await readFile(file);
        response.writeHead(200, { 'Content-Type': types[extname(file)] ?? 'application/octet-stream' });
        response.end(body);
    } catch {
        response.writeHead(404).end('not found');
    }
}).listen(port, () => console.log(`http://localhost:${port}/`));
