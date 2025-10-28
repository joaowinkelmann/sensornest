<template>
  <q-layout>
    <q-page-container>
      <q-page class="flex flex-center bg-grey-2">
        <q-card style="width: 400px">
          <q-card-section class="q-pa-lg">
            <div class="text-h5 q-mb-md text-center">{{ t('login.title') }}</div>
            <q-form @submit.prevent="handleLogin" class="q-gutter-md">
              <q-input
                filled
                v-model="email"
                :label="t('login.emailLabel')"
                lazy-rules
                type="email"
                :rules="[ val => val && val.length > 0 || t('login.emailRequired')]"
              />

              <q-input
                filled
                type="password"
                v-model="password"
                :label="t('login.passwordLabel')"
                lazy-rules
                :rules="[ val => val && val.length > 0 || t('login.passwordRequired')]"
              />

              <div v-if="authStore.error" class="text-negative text-center q-my-sm">
                {{ authStore.error }}
              </div>

              <div>
                <q-btn :label="t('login.signInButton')" type="submit" color="primary" class="full-width" :loading="authStore.loading" />
                <q-btn :label="t('login.registerButton')" flat color="primary" class="full-width q-mt-sm" @click="goToRegister" />
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
import { useAuthStore } from '@/stores/auth';
import { useI18n } from 'vue-i18n';

const { t } = useI18n();
const authStore = useAuthStore();
const router = useRouter();

const email = ref('test@example.com');
const password = ref('password123');

const handleLogin = () => {
  authStore.login({ email: email.value, password: password.value });
};

const goToRegister = () => {
  router.push('/register');
};
</script>
