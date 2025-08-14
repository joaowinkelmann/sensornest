import { createRouter, createWebHistory } from 'vue-router';
import DashboardView from '../views/DashboardView.vue';
import { useAuthStore } from '@/stores/auth';

const router = createRouter({
  history: createWebHistory(import.meta.env.BASE_URL),
  routes: [
    {
      path: '/',
      name: 'dashboard',
      component: DashboardView,
    },
    {
      path: '/:catchAll(.*)',
      redirect: '/',
    },
  ],
});

// Navigation Guard
router.beforeEach(async (to, from, next) => {
  const authStore = useAuthStore();

  // Try to fetch profile if we have a token but no user object yet.
  // This is useful for page reloads.
  if (authStore.token && !authStore.user) {
    await authStore.getProfile();
  }
  
  next();
});

export default router;