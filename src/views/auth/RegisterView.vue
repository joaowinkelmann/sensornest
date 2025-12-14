<template>
  <q-layout>
    <q-page-container>
      <q-page class="flex flex-center bg-grey-2">
        <q-card style="width: 400px">
          <q-card-section class="q-pa-lg">
            <div class="text-h5 q-mb-md text-center">{{ t('register.title') }}</div>
            <q-form @submit.prevent="handleRegister" class="q-gutter-md">
              <q-input filled v-model="name" :label="t('register.nameLabel')" :rules="[val => !!val || t('register.nameRequired')]" />
              <q-input filled v-model="email" :label="t('register.emailLabel')" type="email" :rules="[val => !!val || t('register.emailRequired')]" />
              <q-input filled v-model="password" :label="t('register.passwordLabel')" type="password" :rules="[val => !!val || t('register.passwordRequired')]" />

              <!-- Confirm Password -->
              <q-input
                filled
                v-model="confirmPassword"
                :label="t('register.confirmPasswordLabel')"
                type="password"
                :rules="[
                  val => !!val || t('register.confirmPasswordRequired'),
                  val => val === password || t('register.passwordsMustMatch')
                ]"
              />

              <div v-if="error" class="text-negative text-center q-my-sm">{{ error }}</div>
              <div v-if="success" class="text-positive text-center q-my-sm">{{ success }}</div>

              <div>
                <q-btn :label="t('register.registerButton')" type="submit" color="primary" class="full-width" :loading="loading" />
                <q-btn :label="t('register.loginButton')" flat color="primary" class="full-width q-mt-sm" @click="goToLogin" />
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
import { useI18n } from 'vue-i18n';

const { t } = useI18n();
const router = useRouter();

const name = ref('');
const email = ref('');
const password = ref('');
const confirmPassword = ref('');
const loading = ref(false);
const error = ref('');
const success = ref('');

const handleRegister = async () => {
  error.value = '';
  success.value = '';
  loading.value = true;

  if (!password.value || !confirmPassword.value || password.value !== confirmPassword.value) {
    loading.value = false;
    error.value = t('register.passwordsMustMatch');
    return;
  }

  try {
    await apiClient.post('/users/register', {
      name: name.value,
      email: email.value,
      password: password.value,
    });
    success.value = t('register.successMessage');
    setTimeout(() => {
      router.push('/login');
    }, 1500);
  } catch (e) {
    console.log(e);
    error.value = t('register.errorMessage');
  } finally {
    loading.value = false;
  }
};

const goToLogin = () => {
  router.push('/login');
};
</script>
