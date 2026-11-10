<template>
  <q-dialog :model-value="show" @update:model-value="$emit('close')">
    <q-card style="min-width: 400px; max-width: 90vw;">
      <q-card-section>
        <div class="text-h6">{{ t('login.recoverPasswordTitle') }}</div>
        <div class="text-subtitle2 q-mt-sm">{{ t('login.recoverPasswordInstructions') }}</div>

        <q-input
          v-model="localEmail"
          :label="t('login.emailLabel')"
          type="email"
          filled
          class="q-mt-md"
          :rules="[ val => !!val || t('login.emailRequired') ]"
        />
        <div v-if="error" class="text-negative q-mt-sm">{{ error }}</div>
      </q-card-section>

      <q-card-actions align="right">
        <q-btn flat :label="t('common.cancel')" color="primary" @click="close" />
        <q-btn :label="t('login.recoverButton')" color="primary" @click="sendRecovery" :loading="loading" />
      </q-card-actions>
    </q-card>
  </q-dialog>
</template>

<script setup>
import { ref, watch } from 'vue';
import { useQuasar } from 'quasar';
import apiClient from '@/api';
import { useI18n } from 'vue-i18n';

const { t } = useI18n();
const $q = useQuasar();

const props = defineProps({
  show: { type: Boolean, default: false },
  initialEmail: { type: String, default: '' }
});

const emit = defineEmits(['close']);

const localEmail = ref(props.initialEmail || '');
const loading = ref(false);
const error = ref(null);

watch(() => props.show, (val) => {
  if (val) {
    // reset when opened
    error.value = null;
    loading.value = false;
    localEmail.value = props.initialEmail || '';
  }
});

const close = () => {
  emit('close');
};

const sendRecovery = async () => {
  if (!localEmail.value) {
    error.value = t('login.emailRequired');
    return;
  }
  loading.value = true;
  error.value = null;
  try {
    await apiClient.post('/users/recover-password', { email: localEmail.value });
    $q.notify({ type: 'positive', message: t('login.recoverPasswordSent') });
    close();
  } catch (e) {
    console.error('Password recovery failed:', e);
    error.value = t('login.recoverPasswordError') || 'Failed to send recovery e-mail.';
    $q.notify({ type: 'negative', message: error.value });
  } finally {
    loading.value = false;
  }
};
</script>

<style scoped>
/* small spacing tweaks */
.q-mt-md { margin-top: 12px; }
</style>