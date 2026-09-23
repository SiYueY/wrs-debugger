<script setup lang="ts">
import type { RadioKind } from '../domain/radio';
import { t } from '../i18n';

defineProps<{ mode: RadioKind; message: string }>();
defineEmits<{ 'update:mode': [value: RadioKind] }>();
</script>

<template>
  <div class="view">
    <section class="configuration-shell">
      <nav class="mode-nav" :aria-label="t('modulation')">
        <button :class="{ active: mode === 'lora' }" @click="$emit('update:mode', 'lora')">
          LoRa
        </button>
        <button :class="{ active: mode === 'gfsk' }" @click="$emit('update:mode', 'gfsk')">
          GFSK
        </button>
      </nav>
      <section class="configuration-card">
        <section class="identity-panel">
          <div class="identity-row">
            <slot name="identity-primary" />
            <div class="divider"></div>
            <slot name="identity-secondary" />
          </div>
        </section>
        <section class="parameter-panel">
          <div class="parameter-header">
            <div class="panel-tag">{{ t('communicationParameters') }}</div>
          </div>
          <div class="parameter-content"><slot /></div>
          <div class="parameter-actions">
            <span :class="{ error: message && message !== t('operationSucceeded') }">{{
              message
            }}</span>
            <slot name="actions" />
          </div>
        </section>
      </section>
    </section>
  </div>
</template>

<style scoped>
.view {
  min-width: 0;
  height: 100%;
  min-height: 0;
}
.configuration-shell {
  position: relative;
  height: 100%;
  min-height: 0;
}
.mode-nav {
  display: flex;
  position: absolute;
  z-index: 2;
  top: 0;
  left: 0;
  width: 82px;
  flex-direction: column;
  gap: 10px;
}
.mode-nav button {
  width: 82px;
  height: 42px;
  border: 0;
  border-radius: 21px 0 0 21px;
  background: #c7c7c7;
  color: #202020;
  font-size: 16px;
  cursor: pointer;
}
.mode-nav button.active {
  z-index: 1;
  border: 1px solid #d0d0d0;
  border-right: 0;
  background: #f3f3f3;
}
.configuration-card {
  display: flex;
  position: relative;
  z-index: 1;
  height: 100%;
  margin-left: 82px;
  flex-direction: column;
  min-width: 0;
  min-height: 0;
  gap: 10px;
  padding: 12px;
  border: 1px solid #d0d0d0;
  border-left: 0;
  border-radius: 0 6px 6px 0;
  background: #f3f3f3;
}
.identity-panel,
.parameter-panel {
  min-height: 0;
  border: 1px solid #e1e1e1;
  border-radius: 6px;
  background: #fff;
}
.identity-panel {
  flex: 0 0 72px;
}
.parameter-panel {
  display: flex;
  flex: 1;
  flex-direction: column;
  overflow: hidden;
}
.identity-row {
  display: grid;
  grid-template-columns: minmax(0, 1fr) 1px minmax(0, 1fr);
  align-items: center;
  gap: 28px;
  min-height: 70px;
  padding: 8px 34px;
}
.identity-row :deep(.identity-group) {
  display: grid;
  grid-template-columns: max-content 108px max-content;
  align-items: center;
  gap: 12px;
}
.identity-row :deep(.identity-group label),
.panel-tag {
  border-radius: 3px;
  background: #050505;
  color: #fff;
  padding: 4px 8px;
  font-size: 14px;
  white-space: nowrap;
}
.identity-row :deep(.identity-group output),
.identity-row :deep(.identity-group input) {
  box-sizing: border-box;
  width: 108px;
  height: var(--field-height);
  border: 1px solid var(--input-border);
  border-radius: 3px;
  color: #555;
  padding: 4px 7px;
  text-align: center;
}
.identity-row :deep(.identity-group output) {
  display: flex;
  align-items: center;
  justify-content: center;
  background: #f4f4f4;
}
.identity-row :deep(.pin-actions) {
  display: flex;
  align-items: center;
  gap: 8px;
  white-space: nowrap;
}
.divider {
  width: 1px;
  height: 42px;
  background: #d5d5d5;
}
.parameter-header {
  flex: 0 0 auto;
  padding: 12px 32px 0;
}
.parameter-content {
  min-width: 0;
  min-height: 0;
  flex: 1;
  overflow: auto;
  padding: 0 32px 10px;
}
.panel-tag {
  display: inline-block;
  margin-bottom: 12px;
  padding: 2px 12px;
  font-size: 12px;
  line-height: 1.6;
}
.parameter-actions {
  display: flex;
  align-items: center;
  justify-content: flex-end;
  gap: 10px;
  flex: 0 0 48px;
  padding: 6px 32px 10px;
  background: #fff;
}
.parameter-actions :deep(.dbg-btn) {
  min-width: var(--parameter-button-width);
}
.parameter-actions > span {
  margin-right: auto;
  color: var(--ok);
}
.parameter-actions > .error {
  color: var(--danger);
}
@media (max-width: 920px) {
  .identity-panel {
    flex-basis: auto;
  }
  .identity-row {
    grid-template-columns: minmax(0, 1fr);
    gap: 10px;
    min-height: 0;
    padding: 12px 24px;
  }
  .divider {
    display: none;
  }
  .parameter-header {
    padding: 12px 24px 0;
  }
  .parameter-content {
    padding: 0 24px 10px;
  }
  .parameter-actions {
    padding: 6px 24px 10px;
  }
}
@media (max-width: 680px) {
  .mode-nav,
  .mode-nav button {
    width: 70px;
  }
  .configuration-card {
    margin-left: 70px;
  }
  .identity-row :deep(.identity-group) {
    grid-template-columns: max-content minmax(0, 108px);
  }
  .identity-row :deep(.identity-group .dbg-btn),
  .identity-row :deep(.pin-actions) {
    grid-column: 1 / -1;
    justify-self: start;
  }
}
</style>
