<template>
  <q-dialog :model-value="show" persistent>
    <q-card style="width: 400px">
      <q-card-section class="row items-center q-pb-none">
        <div class="text-h6">Register</div>
        <q-space />
        <q-btn icon="close" flat round dense v-close-popup />
      </q-card-section>

      <q-card-section>
        <q-form @submit.prevent="handleRegister" class="q-gutter-md">
          <q-input filled v-model="name" label="Name" :rules="[val => !!val || 'Name is required']" />
          <q-input filled v-model="email" label="Email" type="email" :rules="[val => !!val || 'Email is required']" />
          <q-input filled v-model="password" label="Password" type="password" :rules="[val => !!val || 'Password is required']" />

          <div v-if="error" class="text-negative text-center q-my-sm">{{ error }}</div>
          <div v-if="success" class="text-positive text-center q-my-sm">{{ success }}</div>

          <div>
            <q-btn label="Register" type="submit" color="primary" class="full-width" :loading="loading" />
            <q-btn label="Back to login" flat color="primary" class="full-width q-mt-sm" @click="$emit('show-login')" />
          </div>
        </q-form>
      </q-card-section>
    </q-card>
  </q-dialog>
</template>

<script setup>
import { ref } from 'vue';
import apiClient from '@/api';

defineProps({
  show: Boolean,
});
const emit = defineEmits(['show-login']);

const name = ref('');
const email = ref('');
const password = ref('');
const loading = ref(false);
const error = ref('');
const success = ref('');

const handleRegister = async () => {
  error.value = '';
  success.value = '';
  loading.value = true;
  try {
    const response = await apiClient.post('/users/register', {
      name: name.value,
      email: email.value,
      password: password.value,
    });
    success.value = response.data.message + ' Please log in.';
    setTimeout(() => {
      emit('show-login');
    }, 1500);
  } catch (e) {
    console.log(e);
    error.value = 'Registration failed.';
  } finally {
    loading.value = false;
  }
};
</script>