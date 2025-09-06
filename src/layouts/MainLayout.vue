<template>
  <q-layout view="hHh lpR fFf">
    <q-header
      elevated
      class="bg-primary text-white"
      :style="{ position: 'absolute', top: 0, left: 0, right: 0, zIndex: 1000 }"
    >
      <q-toolbar>
        <q-btn
          dense
          flat
          round
          :icon="navbarVisible ? 'visibility_off' : 'visibility'"
          @click="toggleNavbar"
        />

        <template v-if="navbarVisible">
          <q-toolbar-title> TempMesh </q-toolbar-title>

          <q-btn flat to="/meshes">Meshes</q-btn>
          <q-btn flat to="/reports">My Reports</q-btn>
          <q-btn flat to="/data">View Data</q-btn>
          <q-btn flat to="/map-editor">Map Editor</q-btn>

          <q-btn flat round>
            <q-avatar icon="person" />
            <q-menu>
              <q-list style="min-width: 100px">
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
        </template>
        <template v-else>
          <q-space />
          <q-toolbar-title> TempMesh </q-toolbar-title>
        </template>
      </q-toolbar>
    </q-header>

    <q-page-container :style="{ paddingTop: '50px' }">
      <router-view />
    </q-page-container>
  </q-layout>
</template>

<script setup>
import { ref, watch } from "vue";
import { useAuthStore } from "@/stores/auth";
import { useRouter, useRoute } from "vue-router";

const authStore = useAuthStore();
const router = useRouter();
const route = useRoute();

const navbarVisible = ref(true);

const toggleNavbar = () => {
  navbarVisible.value = !navbarVisible.value;
};

const handleLogout = () => {
  authStore.logout();
  router.push("/login");
};

watch(
  () => route.meta,
  (meta) => {
    if (meta.navbarHidden) {
      navbarVisible.value = false;
    } else {
      navbarVisible.value = true;
    }
  },
  { immediate: true }
);
</script>

<style scoped>
.q-header {
  transition: transform 0.3s ease-in-out;
}

.q-page-container {
  transition: padding-top 0.3s ease-in-out;
}
</style>
