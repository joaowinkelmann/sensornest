<template>
  <q-layout view="hHh lpR fFf">
    <q-header elevated class="bg-primary text-white">
      <q-toolbar>
        <q-btn dense flat round icon="menu" @click="toggleLeftDrawer" />
        <q-toolbar-title>
          TempMesh
        </q-toolbar-title>
        <q-btn flat @click="handleLogout">Logout</q-btn>
      </q-toolbar>
    </q-header>

    <q-drawer v-model="leftDrawerOpen" side="left" overlay behavior="mobile" elevated>
      <q-list>
        <q-item clickable v-ripple to="/">
          <q-item-section avatar>
            <q-icon name="dashboard" />
          </q-item-section>
          <q-item-section>
            Dashboard
          </q-item-section>
        </q-item>

        <q-item clickable v-ripple to="/map-editor">
          <q-item-section avatar>
            <q-icon name="map" />
          </q-item-section>
          <q-item-section>
            Map Editor
          </q-item-section>
        </q-item>

        <!-- Add more links here -->

      </q-list>
    </q-drawer>

    <q-page-container>
      <router-view />
    </q-page-container>
  </q-layout>
</template>

<script setup>
import { ref } from 'vue';
import { useAuthStore } from '@/stores/auth';
import { useRouter } from 'vue-router';

const authStore = useAuthStore();
const router = useRouter();

const leftDrawerOpen = ref(false);

const toggleLeftDrawer = () => {
  leftDrawerOpen.value = !leftDrawerOpen.value;
};

const handleLogout = () => {
  authStore.logout();
  router.push('/login');
};
</script>
