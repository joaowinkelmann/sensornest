<template>
  <div class="min-h-screen flex items-center justify-center bg-gray-50 py-12 px-4 sm:px-6 lg:px-8">
    <div class="max-w-md w-full space-y-8 p-10 bg-white shadow-lg rounded-xl">
      <div>
        <h2 class="mt-6 text-center text-3xl font-extrabold text-gray-900">
          Register
        </h2>
      </div>
      <form class="mt-8 space-y-6" @submit.prevent="handleRegister">
        <div class="relative">
          <label for="name" class="sr-only">Name</label>
          <input 
            v-model="name" 
            id="name" 
            name="name" 
            type="text" 
            required 
            class="w-full px-3 py-2 border border-gray-300 placeholder-gray-500 text-gray-900 rounded-md focus:outline-none focus:ring-indigo-500 focus:border-indigo-500 sm:text-sm" 
            placeholder="Name"
          >
        </div>
        <div class="relative">
          <label for="email-address" class="sr-only">Email address</label>
          <input 
            v-model="email" 
            id="email-address" 
            name="email" 
            type="email" 
            required 
            class="w-full px-3 py-2 border border-gray-300 placeholder-gray-500 text-gray-900 rounded-md focus:outline-none focus:ring-indigo-500 focus:border-indigo-500 sm:text-sm" 
            placeholder="Email address"
          >
        </div>
        <div class="relative">
          <label for="password" class="sr-only">Password</label>
          <input 
            v-model="password" 
            id="password" 
            name="password" 
            type="password" 
            required 
            class="w-full px-3 py-2 border border-gray-300 placeholder-gray-500 text-gray-900 rounded-md focus:outline-none focus:ring-indigo-500 focus:border-indigo-500 sm:text-sm" 
            placeholder="Password"
          >
        </div>
        <div v-if="error" class="text-red-500 text-sm text-center font-medium">
          {{ error }}
        </div>
        <div v-if="success" class="text-green-500 text-sm text-center font-medium">
          {{ success }}
        </div>
        <div>
          <button 
            type="submit" 
            :disabled="loading" 
            class="group relative w-full flex justify-center py-2 px-4 border border-transparent text-sm font-medium rounded-md text-white bg-indigo-600 hover:bg-indigo-700 focus:outline-none focus:ring-2 focus:ring-offset-2 focus:ring-indigo-500 disabled:bg-indigo-400 disabled:cursor-not-allowed"
          >
            <span v-if="loading">Registering...</span>
            <span v-else>Register</span>
          </button>
        </div>
      </form>
    </div>
  </div>
</template>

<script setup>
import { ref } from 'vue'
import apiClient from '@/api'
import { useRouter } from 'vue-router'

const name = ref('')
const email = ref('')
const password = ref('')
const loading = ref(false)
const error = ref('')
const success = ref('')
const router = useRouter()

const handleRegister = async () => {
  error.value = ''
  success.value = ''
  loading.value = true
  try {
    const response = await apiClient.post('/users/register', {
      name: name.value,
      email: email.value,
      password: password.value,
    })
    success.value = response.data.message
    setTimeout(() => {
      router.push('/login')
    }, 1200)
  } catch (e) {
    error.value = 'Registration failed.'
  } finally {
    loading.value = false
  }
}
</script>