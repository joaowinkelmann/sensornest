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

              <!-- Forgot password -->
              <div class="text-center q-mt-sm">
                <q-btn flat color="primary" @click="showRecovery = true" :label="t('login.forgotPassword')" />
              </div>
            </q-form>
          </q-card-section>
        </q-card>

        <!-- Recovery dialog -->
        <PasswordRecoveryDialog :show="showRecovery" :initial-email="email" @close="showRecovery = false" />
        
        <!-- Reset Password Dialog -->
        <ResetPasswordDialog 
          :show="showReset" 
          :token="resetToken" 
          @close="showReset = false" 
          @success="showReset = false" 
        />
      </q-page>
    </q-page-container>
  </q-layout>
</template>

<script setup>
import { ref, onMounted } from 'vue';
import { useRouter, useRoute } from 'vue-router';
import { useAuthStore } from '@/stores/auth';
import { useQuasar } from 'quasar';
import apiClient from '@/api';
import { useI18n } from 'vue-i18n';
import PasswordRecoveryDialog from '@/components/PasswordRecoveryDialog.vue';
import ResetPasswordDialog from '@/components/ResetPasswordDialog.vue';

const { t } = useI18n();
const $q = useQuasar();
const authStore = useAuthStore();
const router = useRouter();
const route = useRoute();

const email = ref('');
const password = ref('');

const showRecovery = ref(false);
const showReset = ref(false);
const resetToken = ref('');

const handleLogin = () => {
  authStore.login({ email: email.value, password: password.value });
};

const goToRegister = () => {
  router.push('/register');
};

const checkResetToken = async () => {
  const token = route.query.token;
  if (token && typeof token === 'string') {
    try {
      // Validate token
      const response = await apiClient.get('/users/reset-password/check', { params: { token } });
      if (response.data && response.data.valid) {
        resetToken.value = token;
        showReset.value = true;
      } else {
         $q.notify({ type: 'negative', message: t('resetPassword.tokenInvalid') });
      }
    } catch (e) {
      console.error(e);
      $q.notify({ type: 'negative', message: t('resetPassword.tokenInvalid') });
    }
    // Clean up URL ?
    router.replace({ query: null });
  }
};

onMounted(() => {
  checkResetToken();
});
</script>
