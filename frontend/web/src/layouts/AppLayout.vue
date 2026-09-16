<script setup lang="ts">
import AppHeader from '../components/AppHeader.vue';
import AppSidebar from '../components/AppSidebar.vue';
</script>
<template>
  <main class="app">
    <AppSidebar />
    <section class="shell">
      <AppHeader />
      <div class="content"><RouterView /></div>
      <footer>
        WRS Debugger v0.1.0
        <span>Mock environment · Backend not connected</span>
      </footer>
    </section>
  </main>
</template>
<style scoped>
.app {
  display: flex;
  min-height: 100vh;
}
.shell {
  display: flex;
  flex: 1;
  min-width: 0;
  flex-direction: column;
}
.content {
  flex: 1;
  padding: 28px;
  overflow: auto;
}
footer {
  height: 34px;
  padding: 9px 28px;
  color: var(--app-muted);
  font-size: 12px;
  background: #fff;
  border-top: 1px solid var(--app-border);
}
footer span {
  margin-left: 18px;
}
</style>
