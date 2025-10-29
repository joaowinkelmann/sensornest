<template>
  <q-layout>
    <q-page-container>
      <q-page class="flex flex-center bg-grey-2">
        <q-card style="width: 400px">
          <q-card-section class="q-pa-lg">
            <div class="text-h5 q-mb-md text-center">{{ t('resetPassword.title') }}</div>

            <div v-if="checkingToken" class="column items-center q-my-md">
              <q-spinner size="42px" />
              <div class="q-mt-md">{{ t('resetPassword.tokenChecking') }}</div>
            </div>

            <div v-else-if="!tokenValid" class="column items-center q-my-md">
              <q-icon name="error" color="negative" size="42px" />
              <div class="text-negative q-mt-sm text-center">{{ t('resetPassword.tokenInvalid') }}</div>
              <q-btn flat color="primary" class="q-mt-md" :label="t('resetPassword.backToLogin')" @click="goToLogin" />
            </div>

            <q-form v-else @submit.prevent="handleSubmit" class="q-gutter-md">
              <q-input
                filled
                v-model="newPassword"
                :label="t('resetPassword.newPasswordLabel')"
                type="password"
                :rules="[
                  val => !!val || t('register.passwordRequired'),
                  val => (val && val.length >= 6) || 'Min 6 characters'
                ]"
              />
              <q-input
                filled
                v-model="confirmPassword"
                :label="t('resetPassword.confirmPasswordLabel')"
                type="password"
                :rules="[
                  val => !!val || t('register.confirmPasswordRequired'),
                  val => val === newPassword || t('register.passwordsMustMatch')
                ]"
              />

              <div v-if="error" class="text-negative text-center q-my-sm">{{ error }}</div>
              <div v-if="success" class="text-positive text-center q-my-sm">{{ success }}</div>

              <div>
                <q-btn :label="t('resetPassword.submitButton')" type="submit" color="primary" class="full-width" :loading="loading" />
                <q-btn flat color="primary" class="full-width q-mt-sm" :label="t('resetPassword.backToLogin')" @click="goToLogin" />
              </div>
            </q-form>
          </q-card-section>
        </q-card>
      </q-page>
    </q-page-container>
  </q-layout>
</template>

<script setup>
import { ref, onMounted } from 'vue';
import { useRoute, useRouter } from 'vue-router';
import apiClient from '@/api';
import { useI18n } from 'vue-i18n';

const { t } = useI18n();
const route = useRoute();
const router = useRouter();

const token = ref('');
const checkingToken = ref(true);
const tokenValid = ref(false);
const loading = ref(false);
const error = ref('');
const success = ref('');
const newPassword = ref('');
const confirmPassword = ref('');

const goToLogin = () => {
  router.push('/login');
};

const validateToken = async () => {
  const qToken = route.query.token;
  token.value = typeof qToken === 'string' ? qToken : '';
  if (!token.value) {
    tokenValid.value = false;
    checkingToken.value = false;
    return;
  }
  try {
    await apiClient.get('/users/reset-password/check', { params: { token: token.value } });
    tokenValid.value = true;
  } catch {
    tokenValid.value = false;
  } finally {
    checkingToken.value = false;
  }
};

const handleSubmit = async () => {
  error.value = '';
  success.value = '';
  if (!newPassword.value || !confirmPassword.value || newPassword.value !== confirmPassword.value) {
    error.value = t('register.passwordsMustMatch');
    return;
  }
  loading.value = true;
  try {
    await apiClient.post('/users/reset-password', {
      token: token.value,
      newPassword: newPassword.value
    });
    success.value = t('resetPassword.success');
    setTimeout(() => {
      router.push('/login');
    }, 1500);
  } catch (e) {
    console.error(e);
    error.value = t('resetPassword.error');
  } finally {
    loading.value = false;
  }
};

onMounted(validateToken);
</script>