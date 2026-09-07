import { defineConfig } from 'vite';
import { svelte } from '@sveltejs/vite-plugin-svelte';
import tailwindcss from '@tailwindcss/vite';
import { fileURLToPath, URL } from 'node:url';
export default defineConfig({
  plugins: [svelte(), tailwindcss()],
  base: './',
  resolve: { alias: { '@juce': fileURLToPath(new URL('../JUCE/modules/juce_gui_extra/native/typescript/webview-interop/dist/index.js', import.meta.url)) } },
  server: { host: '127.0.0.1', port: 5175, strictPort: true, cors: { origin: [/^juce:/, /^https?:\/\/juce\./, /^http:\/\/localhost:5175$/] } },
  build: { outDir: 'dist', emptyOutDir: true },
});
