import { createRouter, createWebHashHistory } from 'vue-router';

export const router = createRouter({
  history: createWebHashHistory(),

  routes: [
    {
      path: '/',
      redirect: '/connection',
    },
    {
      path: '/connection',
      component: () => import('../views/ConnectionView.vue'),
    },
    {
      path: '/box',
      component: () => import('../views/BoxConfigView.vue'),
    },
    {
      path: '/receiver',
      component: () => import('../views/ReceiverConfigView.vue'),
    },
    { path: '/:pathMatch(.*)*', redirect: '/connection' },
  ],
});
