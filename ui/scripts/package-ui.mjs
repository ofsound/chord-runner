import { readdir, readFile, mkdir, writeFile } from 'node:fs/promises';
import { join, relative, resolve } from 'node:path';
import { zipSync } from 'fflate';
const root = resolve('dist');
const entries = {};
async function collect(directory) {
  for (const entry of (await readdir(directory, { withFileTypes: true })).sort((a, b) => a.name.localeCompare(b.name))) {
    const path = join(directory, entry.name);
    if (entry.isDirectory()) await collect(path);
    else entries[relative(root, path).replaceAll('\\', '/')] = [new Uint8Array(await readFile(path)), { mtime: new Date('2020-01-01T00:00:00Z') }];
  }
}
await collect(root);
await mkdir('../assets/webview', { recursive: true });
await writeFile('../assets/webview/ui.zip', zipSync(entries, { level: 9 }));
