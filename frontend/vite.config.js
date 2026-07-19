import { defineConfig } from 'vite';
import { svelte } from '@sveltejs/vite-plugin-svelte';

export default defineConfig({
  plugins: [
    svelte()
  ],
  build: {
    // Output directly to Arduino LittleFS data directory
    outDir: '../twESP32GPS/data',
    emptyOutDir: true,
    rollupOptions: {
      output: {
        // Single chunk — all JS/CSS bundled together for simpler LittleFS layout
        manualChunks: undefined,
        // Deterministic filenames (no hash) for easy LittleFS serving
        entryFileNames:  'assets/[name].js',
        chunkFileNames:  'assets/[name].js',
        assetFileNames:  'assets/[name].[ext]',
      },
    },
    // Inline small assets (icons, fonts) into the bundle
    assetsInlineLimit: 1024 * 10,
  },
  server: {
    // Dev server: proxy /api/gps to the actual ESP32
    // Change target to your ESP32's IP address for live development
    proxy: {
      '/api': {
        target: 'http://192.168.1.100',
        changeOrigin: true,
      },
    },
  },
});
