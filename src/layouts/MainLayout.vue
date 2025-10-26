<template>
  <q-layout view="hHh lpR fFf">
    <Sidebar v-model="leftDrawerOpen" :user="authStore.user" @logout="handleLogout" />

    <q-page-container>
      <router-view />
    </q-page-container>
  </q-layout>
</template>

<script setup>
import { ref } from 'vue';
import Sidebar from '@/components/Sidebar.vue';
import { useAuthStore } from '@/stores/auth';
import { useRouter } from 'vue-router';

const authStore = useAuthStore();
const router = useRouter();

const leftDrawerOpen = ref(true);

const handleLogout = () => {
  authStore.logout();
  router.push('/login');
};
</script>
