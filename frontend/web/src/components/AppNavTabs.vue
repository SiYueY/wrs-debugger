<script setup lang="ts">
import { useRoute, useRouter } from 'vue-router';
import { t } from '../i18n';
const route = useRoute();
const router = useRouter();
const tabs = [
  { path: '/transmitter', key: 'box' as const },
  { path: '/receiver', key: 'receiver' as const },
];
</script>
<template>
  <nav class="nav" role="tablist">
    <button
      v-for="tab in tabs"
      :key="tab.path"
      :class="{ active: route.path === tab.path }"
      @click="router.push(tab.path)"
    >
      {{ t(tab.key) }}
    </button>
  </nav>
</template>
<style scoped>
.nav {
  display: inline-flex;
  width: fit-content;
  height: var(--navigation-height);
  border-radius: 22px;
  overflow: hidden;
  background: var(--track);
}
button {
  width: var(--navigation-width);
  border: 0;
  border-radius: 22px;
  background: transparent;
  color: #fff;
  font: inherit;
  cursor: pointer;
}
button.active {
  background: var(--brand);
}
</style>
