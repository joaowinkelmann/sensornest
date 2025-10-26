<template>
  <q-layout>
    <q-page-container>
      <q-page class="flex flex-center bg-grey-2">
        <q-card style="width: 400px">
          <q-card-section class="q-pa-lg">
            <div class="text-h5 q-mb-md text-center">Create an Account</div>
            <q-form @submit.prevent="handleRegister" class="q-gutter-md">
              <q-input filled v-model="name" label="Name" :rules="[val => !!val || 'Name is required']" />
              <q-input filled v-model="email" label="Email" type="email" :rules="[val => !!val || 'Email is required']" />
              <q-input filled v-model="password" label="Password" type="password" :rules="[val => !!val || 'Password is required']" />

              <div v-if="error" class="text-negative text-center q-my-sm">{{ error }}</div>
              <div v-if="success" class="text-positive text-center q-my-sm">{{ success }}</div>

              <div>
                <q-btn label="Register" type="submit" color="primary" class="full-width" :loading="loading" />
                <q-btn label="Back to login" flat color="primary" class="full-width q-mt-sm" @click="goToLogin" />
              </div>
            </q-form>
          </q-card-section>
        </q-card>
      </q-page>
    </q-page-container>
  </q-layout>
</template>

<script setup>
import { ref } from 'vue';
import { useRouter } from 'vue-router';
import apiClient from '@/api';

const router = useRouter();

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
    await apiClient.post('/users/register', {
      name: name.value,
      email: email.value,
      password: password.value,
    });
    success.value = 'Registration successful! Please log in.';
    setTimeout(() => {
      router.push('/login');
    }, 1500);
  } catch (e) {
    console.log(e);
    error.value = 'Registration failed.';
  } finally {
    loading.value = false;
  }
};

const goToLogin = () => {
  router.push('/login');
};
</script>
