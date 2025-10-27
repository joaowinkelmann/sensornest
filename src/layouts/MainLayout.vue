<template>
  <q-layout view="hHh lpR fFf">
    <MainSidebar v-model="leftDrawerOpen" :user="authStore.user" @logout="handleLogout" />

    <q-page-container>
      <router-view />
    </q-page-container>

    <q-page-sticky
      position="top-left"
      :offset="[8, 8]"
      v-if="$q.screen.lt.md && !leftDrawerOpen"
      class="mobile-fab-container"
    >
      <q-btn fab-mini dense="true" glossy="true" icon="menu" color="primary" @click="toggleLeftDrawer" />
    </q-page-sticky>
  </q-layout>
</template>

<script setup>
import { ref } from 'vue';
import MainSidebar from '@/components/MainSidebar.vue';
import { useAuthStore } from '@/stores/auth';
import { useRouter } from 'vue-router';

const authStore = useAuthStore();
const router = useRouter();

const leftDrawerOpen = ref(true);

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
</style>
