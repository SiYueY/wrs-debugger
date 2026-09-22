<script setup lang="ts">
import MarkdownIt from 'markdown-it';
import { computed, ref, watch } from 'vue';
import { locale, setLocale, t, type Locale } from '../i18n';
const open = defineModel<boolean>('open', { required: true });
const manualOpen = ref(false);
const source = ref<string>();
const failed = ref(false);
const renderer = new MarkdownIt({ html: false, linkify: true, breaks: true });
const rendered = computed(() => (source.value ? renderer.render(source.value) : ''));
async function loadManual() {
  failed.value = false;
  source.value = undefined;
  try {
    const response = await fetch(`./manual/${locale.value}.md`);
    if (!response.ok) throw new Error();
    source.value = await response.text();
  } catch {
    failed.value = true;
  }
  manualOpen.value = true;
}
watch(locale, () => {
  if (manualOpen.value) void loadManual();
});
function update(event: Event) {
  setLocale((event.target as HTMLSelectElement).value as Locale);
}
</script>
<template>
  <button class="settings" :title="t('settings')" @click="open = true">⚙</button>
  <div v-if="open" class="overlay" @click.self="open = false">
    <section class="dialog">
      <header>
        <strong>{{ t('settings') }}</strong
        ><button @click="open = false">×</button>
      </header>
      <label
        >{{ t('language')
        }}<select :value="locale" @change="update">
          <option value="zh-CN">简体中文</option>
          <option value="en-US">English</option>
        </select></label
      ><button class="manual" @click="loadManual">{{ t('manual') }}</button>
    </section>
  </div>
  <div v-if="manualOpen" class="overlay" @click.self="manualOpen = false">
    <section class="manual-dialog">
      <header>
        <strong>{{ t('manual') }}</strong
        ><button @click="manualOpen = false">×</button>
      </header>
      <article v-if="source" v-html="rendered" />
      <p v-else>{{ failed ? t('unavailable') : t('loading') }}</p>
    </section>
  </div>
</template>
<style scoped>
.settings {
  width: 32px;
  height: 32px;
  border: 0;
  border-radius: 16px;
  background: var(--brand);
  color: #fff;
  font-size: 17px;
  cursor: pointer;
}
.overlay {
  position: fixed;
  inset: 0;
  z-index: 10;
  display: grid;
  place-items: center;
  background: #0006;
}
.dialog,
.manual-dialog {
  width: min(560px, 90vw);
  max-height: 80vh;
  overflow: auto;
  padding: 16px;
  background: #fff;
  border-radius: 10px;
}
.manual-dialog {
  width: min(900px, 90vw);
}
header {
  display: flex;
  justify-content: space-between;
  margin-bottom: 16px;
}
header button {
  border: 0;
  background: transparent;
  font-size: 22px;
  cursor: pointer;
}
label {
  display: grid;
  gap: 6px;
}
select,
.manual {
  height: 32px;
  margin-top: 10px;
  padding: 0 8px;
}
.manual {
  border: 0;
  border-radius: 16px;
  background: var(--brand);
  color: #fff;
  cursor: pointer;
}
article :deep(img) {
  max-width: 100%;
}
</style>
