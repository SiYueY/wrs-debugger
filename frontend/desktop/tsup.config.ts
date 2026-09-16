import { defineConfig } from 'tsup';

export default defineConfig({
  entry: {
    'main/index': 'src/main/index.ts',
    'preload/index': 'src/preload/index.ts',
  },

  format: ['cjs'],

  outExtension() {
    return {
      js: '.cjs',
    };
  },

  platform: 'node',
  target: 'node22',

  sourcemap: true,
  clean: true,

  external: ['electron'],
});
