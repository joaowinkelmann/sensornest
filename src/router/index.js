import { createRouter, createWebHistory } from 'vue-router';
import { useAuthStore } from '@/stores/auth';

// Layouts
import MainLayout from '@/layouts/MainLayout.vue';

// Views
import LoginView from '@/views/auth/LoginView.vue';
import RegisterView from '@/views/auth/RegisterView.vue';
import HomeView from '@/views/home/HomeView.vue';

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
        component: () => import('@/views/map-editor/MapEditorView.vue'),
        beforeEnter: async () => {
          const { useMeshesStore } = await import('@/stores/meshes');
          const { useZonesStore } = await import('@/stores/zones');
          const meshesStore = useMeshesStore();
          const zonesStore = useZonesStore();
          if (meshesStore.selectedMeshId) {
            await zonesStore.fetchZones(meshesStore.selectedMeshId);
          }
        }
      },
      {
        path: 'profile',
        name: 'profile',
        component: () => import('@/views/auth/ProfileView.vue'),
      },
      {
        path: 'meshes',
        name: 'meshes',
        component: () => import('@/views/meshes/MeshesView.vue'),
      },
      {
        path: 'devices',
        name: 'devices',
        component: () => import('@/views/devices/DevicesView.vue'),
      },
      {
        path: 'reports',
        name: 'reports',
        component: () => import('@/views/reports/ReportsView.vue'),
      },
      {
        path: 'data',
        name: 'data',
        component: () => import('@/views/data/DataView.vue'),
      },
      {
        path: 'reset-password',
        name: 'reset-password',
        component: LoginView,
      },
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

  // Only fetch profile if needed (once per app session, or if stale/forced)
  if (authStore.token) {
    await authStore.getProfileIfNeeded();
  }

  const requiresAuth = to.matched.some(record => record.meta.requiresAuth);
  const isAuthenticated = authStore.isAuthenticated;

  if (requiresAuth && !isAuthenticated) {
    next('/login');
  } else if ((to.name === 'login' || to.name === 'register' || to.name === 'reset-password') && isAuthenticated) {
    next('/');
  } else {
    next();
  }
});

export default router;