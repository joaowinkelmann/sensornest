// src/router/index.js
import { createRouter, createWebHistory } from 'vue-router';
import LoginView from '../views/LoginView.vue';
import DashboardView from '../views/DashboardView.vue';
// Create a simple HomeView for demonstration
import HomeView from '../views/HomeView.vue';
import { useAuthStore } from '@/stores/auth';

const router = createRouter({
  history: createWebHistory(import.meta.env.BASE_URL),
  routes: [
    {
      path: '/login',
      name: 'login',
      component: LoginView,
    },
    {
      path: '/dashboard',
      name: 'dashboard',
      component: DashboardView,
      meta: { requiresAuth: true },
    },
    {
      path: '/home',
      name: 'home',
      component: HomeView,
    },
    {
      path: '/',
      redirect: '/home',
    },
  ],
});

// Navigation Guard
router.beforeEach(async (to, from, next) => {
  const authStore = useAuthStore();

  // Try to fetch profile if we have a token but no user object yet
  if (authStore.token && !authStore.user) {
    await authStore.getProfile();
  }

  if (to.meta.requiresAuth && !authStore.isAuthenticated) {
    // If route requires auth and user is not logged in, redirect to login
    next({ name: 'login' });
  } else {
    // Otherwise, allow navigation
    next();
  }
});

export default router;