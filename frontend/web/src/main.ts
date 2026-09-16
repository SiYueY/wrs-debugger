import { createApp } from 'vue';
import { createPinia } from 'pinia';
import {
  ElAlert,
  ElButton,
  ElCard,
  ElDescriptions,
  ElDescriptionsItem,
  ElDialog,
  ElForm,
  ElFormItem,
  ElInput,
  ElInputNumber,
  ElMenu,
  ElMenuItem,
  ElOption,
  ElSelect,
  ElStep,
  ElSteps,
  ElSwitch,
  ElTag,
} from 'element-plus';

import 'element-plus/dist/index.css';

import App from './App.vue';
import { router } from './router';
import './style.css';

const app = createApp(App);

app.use(createPinia());
app.use(router);
app.use(ElAlert);
app.use(ElButton);
app.use(ElCard);
app.use(ElDescriptions);
app.use(ElDescriptionsItem);
app.use(ElDialog);
app.use(ElForm);
app.use(ElFormItem);
app.use(ElInput);
app.use(ElInputNumber);
app.use(ElMenu);
app.use(ElMenuItem);
app.use(ElOption);
app.use(ElSelect);
app.use(ElStep);
app.use(ElSteps);
app.use(ElSwitch);
app.use(ElTag);

app.mount('#app');
