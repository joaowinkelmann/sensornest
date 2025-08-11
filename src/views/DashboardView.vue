<template>
  <div class="flex flex-col h-screen bg-gray-100">
    <!-- Header -->
    <header class="bg-white shadow-sm p-4 flex justify-between items-center z-10">
      <h1 class="text-2xl font-bold text-gray-800">TempMesh Dashboard</h1>
      <div v-if="authStore.user" class="flex items-center">
        <span class="text-gray-600 mr-4">Welcome, {{ authStore.user.name }}</span>
        <button @click="authStore.logout()" class="bg-red-500 hover:bg-red-600 text-white font-bold py-2 px-4 rounded-md transition-colors">
          Logout
        </button>
      </div>
    </header>

    <!-- Main Content -->
    <main class="flex-grow p-4 sm:p-6 lg:p-8 overflow-y-auto">
      <div class="max-w-7xl mx-auto">
        <MeshManager />
        <ZoneManager />
      </div>
    </main>
  </div>
</template>

<script setup>
import { useAuthStore } from '@/stores/auth';
import MeshManager from '@/components/MeshManager.vue';
import ZoneManager from '@/components/ZoneManager.vue';

// Initialize the auth store to access user info and logout action
const authStore = useAuthStore();
</script>