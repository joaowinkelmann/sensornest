<template>
  <q-layout view="hHh lpR fFf">
    <MainSidebar v-model="leftDrawerOpen" :user="authStore.user" @logout="handleLogout" />

    <q-page-container :class="{ 'mobile-top-padding': applyTopPadding }">
      <router-view />
    </q-page-container>

    <q-page-sticky
      position="top-left"
      :offset="[8, 8]"
      v-if="$q.screen.lt.md && !leftDrawerOpen"
      class="mobile-fab-container"
    >
      <q-btn 
        fab-mini 
        dense 
        icon="menu" 
        class="glassmorphism-btn"
        @click="toggleLeftDrawer" 
      />
    </q-page-sticky>
  </q-layout>
</template>

<script setup>
import { ref, computed } from 'vue';
import { useQuasar } from 'quasar';
import MainSidebar from '@/components/MainSidebar.vue';
import { useAuthStore } from '@/stores/auth';
import { useRouter, useRoute } from 'vue-router';

const $q = useQuasar();
const route = useRoute();

const authStore = useAuthStore();
const router = useRouter();

const leftDrawerOpen = ref(true);

// Make padding depend only on mobile + route (not on drawer/FAB state)
const isMobile = computed(() => $q.screen.lt.md);
const isMapEditor = computed(() =>
  route.name === 'map-editor' || route.path?.startsWith('/map-editor')
);
const applyTopPadding = computed(() => isMobile.value && !isMapEditor.value);

// FAB still hides when drawer opens, but padding remains to prevent layout jump
// const showMenuFab = computed(() => isMobile.value && !leftDrawerOpen.value);

const handleLogout = () => {
  authStore.logout();
  router.push('/login');
};

const toggleLeftDrawer = () => {
  leftDrawerOpen.value = !leftDrawerOpen.value;
};
</script>

<style scoped>
.mobile-fab-container {
  z-index: 2000; /* Ensure it's above map controls */
}

.glassmorphism-btn {
  background: rgba(45, 45, 45, 0.45) !important;
  color: white !important;
  box-shadow: 0 4px 24px rgba(0, 0, 0, 0.10) !important;
  backdrop-filter: blur(8px);
  border: 1px solid rgba(255, 255, 255, 0.1);
  transition: all 0.2s ease;
}

.glassmorphism-btn:hover {
  background: rgba(45, 45, 45, 0.65) !important;
  transform: scale(1.05);
}

.glassmorphism-btn:active {
  transform: scale(0.95);
}

/* Dark mode support */
@media (prefers-color-scheme: dark) {
  .glassmorphism-btn {
    background: rgba(45, 45, 45, 0.45) !important;
    border: 1px solid rgba(255, 255, 255, 0.08);
    box-shadow: 0 4px 24px rgba(0, 0, 0, 0.35) !important;
  }
}
</style>

<style>
/* Mobile top padding when FAB is visible (except on map-editor) */
@media (max-width: 1023px) {
  .mobile-top-padding {
    padding-top: 64px;
  }
}
</style>
