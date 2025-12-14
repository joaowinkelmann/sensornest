<template>
  <q-dialog :model-value="show" persistent>
    <q-card style="min-width: 400px; max-width: 90vw;">
      <q-card-section>
        <div class="text-h6">{{ t('resetPassword.title') }}</div>
        <div class="text-subtitle2 q-mt-sm">{{ t('resetPassword.instructions') }}</div>

        <q-form @submit.prevent="handleSubmit" class="q-gutter-md q-mt-md">
          <q-input
            filled
            v-model="newPassword"
            :label="t('resetPassword.newPasswordLabel')"
            type="password"
            :rules="[
              val => !!val || t('register.passwordRequired'),
              val => (val && val.length >= 6) || t('register.passwordLength')
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

          <div class="row justify-end q-mt-lg">
             <!-- User cannot cancel a reset process initiated by token, usually. But maybe they want to just go to login. -->
             <q-btn flat :label="t('buttons.cancel')" color="primary" @click="$emit('close')" />
             <q-btn :label="t('resetPassword.submitButton')" type="submit" color="primary" :loading="loading" />
          </div>
        </q-form>
      </q-card-section>
    </q-card>
  </q-dialog>
</template>

<script setup>
import { ref } from 'vue';
import { useQuasar } from 'quasar';
import apiClient from '@/api';
import { useI18n } from 'vue-i18n';

const { t } = useI18n();
const $q = useQuasar();

const props = defineProps({
  show: { type: Boolean, default: false },
  token: { type: String, required: true }
});

const emit = defineEmits(['close', 'success']);

const newPassword = ref('');
const confirmPassword = ref('');
const loading = ref(false);
const error = ref('');

const handleSubmit = async () => {
  error.value = '';
  if (newPassword.value !== confirmPassword.value) {
    error.value = t('register.passwordsMustMatch');
    return;
  }

  loading.value = true;
  try {
    await apiClient.post('/users/reset-password', {
      token: props.token,
      newPassword: newPassword.value
    });
    $q.notify({ type: 'positive', message: t('resetPassword.success') });
    emit('success');
  } catch (e) {
    console.error(e);
    error.value = t('resetPassword.error');
    // If token expired, maybe tell them
    if (e.response && e.response.status === 400) {
        error.value = t('resetPassword.tokenInvalid');
    }
  } finally {
    loading.value = false;
  }
};
</script>
