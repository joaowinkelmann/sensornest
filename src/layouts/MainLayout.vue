<template>
  <q-layout view="hHh lpR fFf">
    <q-header
      elevated
      class="bg-primary text-white"
      :class="{ hidden: !navbarVisible }"
      dense
    >
      <q-toolbar>
        <!-- Nav content -->
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

      <!-- Collapse button (visible when navbar is expanded) -->
      <div v-if="navbarVisible" class="navbar-toggle-wrapper">
        <q-btn
          round
          dense
          color="primary"
          icon="expand_less"
          @click="toggleNavbar"
          aria-label="Collapse navbar"
        />
      </div>
    </q-header>

    <!-- Expand button (visible when navbar is hidden) -->
    <div v-if="!navbarVisible" class="navbar-reveal-wrapper">
      <q-btn
        round
        dense
        color="primary"
        icon="expand_more"
        @click="toggleNavbar"
        aria-label="Expand navbar"
      />
    </div>

    <!-- Let QLayout manage the header offset; do not add extra padding -->
    <q-page-container>
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
    navbarVisible.value = !meta.navbarHidden;
  },
  { immediate: true }
);
</script>

<style scoped>
.q-header {
  transition: transform 0.25s ease-in-out;
}
.q-header.hidden {
  transform: translateY(-100%);
}

/* Collapse button below expanded navbar */
.navbar-toggle-wrapper {
  position: absolute;
  left: 50%;
  bottom: -14px;
  transform: translateX(-50%);
  z-index: 950;
  pointer-events: none; /* don't block content */
}
.navbar-toggle-wrapper :deep(.q-btn) {
  pointer-events: auto; /* only the button is clickable */
}

/* Expand button at the very top when navbar is hidden */
.navbar-reveal-wrapper {
  position: fixed;
  top: 8px;
  left: 50%;
  transform: translateX(-50%);
  z-index: 950; /* below FloatingSidebar (which is 1000) */
  pointer-events: none;
}
.navbar-reveal-wrapper :deep(.q-btn) {
  pointer-events: auto;
}
</style>
