import vue from '@vitejs/plugin-vue';
import { defineConfig } from 'vitest/config';

export default defineConfig({
  define: {
    'import.meta.env.VITE_WRS_API_BASE': JSON.stringify('http://127.0.0.1:8000'),
  },
  plugins: [vue()],
  test: {
    environment: 'jsdom',
    clearMocks: true,
  },
});
