<template>
  <q-layout view="hHh lpR fFf">
    <q-header elevated class="bg-primary text-white">
      <q-toolbar>
        <q-btn v-if="$q.screen.lt.md" dense flat round icon="menu" @click="toggleLeftDrawer" />
        <q-toolbar-title>
          <!-- <q-avatar>
            <img src="">
          </q-avatar> -->
          TempMesh
        </q-toolbar-title>

        <!-- Desktop Navigation -->
        <q-tabs v-if="$q.screen.gt.sm" align="right" class="desktop-tabs">
          <q-route-tab to="/" label="Home" />
          <q-route-tab to="/meshes" label="Meshes" />
          <q-route-tab to="/reports" label="My Reports" />
          <q-route-tab to="/data" label="View Data" />
          <q-route-tab to="/map-editor" label="Map Editor" />
        </q-tabs>

        <q-btn flat round>
          <q-avatar icon="person" />
          <q-menu>
            <q-list style="min-width: 150px">
              <q-item clickable v-close-popup to="/profile">
                <q-item-section>Edit Profile</q-item-section>
              </q-item>
              <q-separator />
              <q-item clickable v-close-popup @click="handleLogout">
                <q-item-section>Logout</q-item-section>
              </q-item>
            </q-list>
          </q-menu>
        </q-btn>
      </q-toolbar>
    </q-header>

    <!-- Mobile Navigation Drawer -->
    <q-drawer v-model="leftDrawerOpen" side="left" bordered>
      <q-list>
        <q-item-label header>Navigation</q-item-label>
        <q-item clickable v-ripple to="/meshes">
          <q-item-section avatar>
            <q-icon name="grid_on" />
          </q-item-section>
          <q-item-section>Meshes</q-item-section>
        </q-item>
        <q-item clickable v-ripple to="/reports">
          <q-item-section avatar>
            <q-icon name="assessment" />
          </q-item-section>
          <q-item-section>My Reports</q-item-section>
        </q-item>
        <q-item clickable v-ripple to="/data">
          <q-item-section avatar>
            <q-icon name="analytics" />
          </q-item-section>
          <q-item-section>View Data</q-item-section>
        </q-item>
        <q-item clickable v-ripple to="/map-editor">
          <q-item-section avatar>
            <q-icon name="map" />
          </q-item-section>
          <q-item-section>Map Editor</q-item-section>
        </q-item>
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
import { useQuasar } from 'quasar';

const authStore = useAuthStore();
const router = useRouter();
const $q = useQuasar();

const leftDrawerOpen = ref(false);

const toggleLeftDrawer = () => {
  leftDrawerOpen.value = !leftDrawerOpen.value;
};

const handleLogout = () => {
  authStore.logout();
  router.push('/login');
};
</script>

<style scoped>
.desktop-tabs {
  margin-right: 20px;
}
</style>
