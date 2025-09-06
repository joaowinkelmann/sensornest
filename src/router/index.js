import { createRouter, createWebHistory } from 'vue-router';
import { useAuthStore } from '@/stores/auth';

// Layouts
import MainLayout from '@/layouts/MainLayout.vue';

// Views
import LoginView from '@/views/auth/LoginView.vue';
import RegisterView from '@/views/auth/RegisterView.vue';
import HomeView from '@/views/home/HomeView.vue';
import MapEditorView from '@/views/map-editor/MapEditorView.vue';

const routes = [
  {
    path: '/login',
    name: 'login',
    component: LoginView,
  },
  {
    path: '/register',
    name: 'register',
    component: RegisterView,
  },
  {
    path: '/',
    component: MainLayout,
    meta: { requiresAuth: true },
    children: [
      {
        path: '',
        name: 'home',
        component: HomeView,
      },
      {
        path: 'map-editor',
        name: 'map-editor',
        component: MapEditorView,
      },
      // TODO: Add other authenticated routes here
    ],
  },
  // Catch-all redirect
  {
    path: '/:catchAll(.*)',
    redirect: '/',
  },
];

const router = createRouter({
  history: createWebHistory(import.meta.env.BASE_URL),
  routes,
});

// Navigation Guard
router.beforeEach(async (to, from, next) => {
  const authStore = useAuthStore();

  // Try to fetch profile if we have a token but no user object yet.
  if (authStore.token && !authStore.user) {
    await authStore.getProfile();
  }

  const requiresAuth = to.matched.some(record => record.meta.requiresAuth);
  const isAuthenticated = authStore.isAuthenticated;

  if (requiresAuth && !isAuthenticated) {
    // Redirect to login page if trying to access a protected route without being authenticated
    next('/login');
  } else if ((to.name === 'login' || to.name === 'register') && isAuthenticated) {
    // Redirect to home if trying to access login/register pages while already authenticated
    next('/');
  } else {
    next();
  }
});

export default router;