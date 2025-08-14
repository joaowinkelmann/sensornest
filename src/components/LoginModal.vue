<template>
  <q-dialog :model-value="show" persistent>
    <q-card style="width: 400px">
      <q-card-section class="row items-center q-pb-none">
        <div class="text-h6">Sign in to TempMesh</div>
        <q-space />
        <q-btn icon="close" flat round dense v-close-popup />
      </q-card-section>

      <q-card-section>
        <q-form @submit.prevent="handleLogin" class="q-gutter-md">
          <q-input
            filled
            v-model="email"
            label="Email address"
            lazy-rules
            type="email"
            :rules="[ val => val && val.length > 0 || 'Please type your email']"
          />

          <q-input
            filled
            type="password"
            v-model="password"
            label="Password"
            lazy-rules
            :rules="[ val => val && val.length > 0 || 'Please type your password']"
          />

          <div v-if="authStore.error" class="text-negative text-center q-my-sm">
            {{ authStore.error }}
          </div>

          <div>
            <q-btn label="Sign in" type="submit" color="primary" class="full-width" :loading="authStore.loading" />
            <q-btn label="Create a new account" flat color="primary" class="full-width q-mt-sm" @click="$emit('show-register')" />
          </div>
        </q-form>
      </q-card-section>
    </q-card>
  </q-dialog>
</template>

<script setup>
import { ref } from 'vue';
import { useAuthStore } from '@/stores/auth';

defineProps({
  show: Boolean,
});
defineEmits(['show-register']);

const authStore = useAuthStore();
const email = ref('test@example.com');
const password = ref('password123');

const handleLogin = () => {
  authStore.login({ email: email.value, password: password.value });
};
</script>