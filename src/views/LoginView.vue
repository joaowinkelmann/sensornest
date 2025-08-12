<template>
  <div class="min-h-screen flex items-center justify-center bg-gray-50 py-12 px-4 sm:px-6 lg:px-8">
    <div class="max-w-md w-full space-y-8 p-10 bg-white shadow-lg rounded-xl">
      <div>
        <h2 class="mt-6 text-center text-3xl font-extrabold text-gray-900">
          Sign in to TempMesh
        </h2>
      </div>
      <form class="mt-8 space-y-6" @submit.prevent="handleLogin">
        <!-- Email Input -->
        <div class="relative">
          <label for="email-address" class="sr-only">Email address</label>
          <input 
            v-model="email" 
            id="email-address" 
            name="email" 
            type="email" 
            autocomplete="email" 
            required 
            class="w-full px-3 py-2 border border-gray-300 placeholder-gray-500 text-gray-900 rounded-md focus:outline-none focus:ring-indigo-500 focus:border-indigo-500 sm:text-sm" 
            placeholder="Email address"
          >
        </div>
        <!-- Password Input -->
        <div class="relative">
          <label for="password" class="sr-only">Password</label>
          <input 
            v-model="password" 
            id="password" 
            name="password" 
            type="password" 
            autocomplete="current-password" 
            required 
            class="w-full px-3 py-2 border border-gray-300 placeholder-gray-500 text-gray-900 rounded-md focus:outline-none focus:ring-indigo-500 focus:border-indigo-500 sm:text-sm" 
            placeholder="Password"
          >
        </div>

        <!-- Error Message -->
        <div v-if="authStore.error" class="text-red-500 text-sm text-center font-medium">
          {{ authStore.error }}
        </div>

        <!-- Submit Button -->
        <div>
          <button 
            type="submit" 
            :disabled="authStore.loading" 
            class="group relative w-full flex justify-center py-2 px-4 border border-transparent text-sm font-medium rounded-md text-white bg-indigo-600 hover:bg-indigo-700 focus:outline-none focus:ring-2 focus:ring-offset-2 focus:ring-indigo-500 disabled:bg-indigo-400 disabled:cursor-not-allowed"
          >
            <span v-if="authStore.loading">Signing in...</span>
            <span v-else>Sign in</span>
          </button>
        </div>

        <!-- Register button -->
        <div class="text-sm text-center">
          <button 
            @click="$router.push('/register')" 
            class="mt-2 font-medium text-indigo-600 hover:text-indigo-500"
          >
            Create a new account
          </button>
        </div>

      </form>
    </div>
  </div>
</template>

<script setup>
import { ref } from 'vue';
import { useAuthStore } from '@/stores/auth';

// Initialize the store
const authStore = useAuthStore();

// Create reactive variables for the form inputs
// Pre-filled with test credentials for convenience
const email = ref('test@example.com');
const password = ref('password123');

// Define the login handler function
const handleLogin = () => {
  const credentials = {
    email: email.value,
    password: password.value,
  };
  // Call the login action from the Pinia store
  authStore.login(credentials);
};
</script>