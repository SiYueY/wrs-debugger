import { createRouter, createWebHashHistory } from 'vue-router';
export const router = createRouter({
  history: createWebHashHistory(),
  routes: [
    { path: '/', redirect: '/transmitter' },
    { path: '/transmitter', component: () => import('../views/TransmitterView.vue') },
    { path: '/receiver', component: () => import('../views/ReceiverView.vue') },
    { path: '/:pathMatch(.*)*', redirect: '/transmitter' },
  ],
});
