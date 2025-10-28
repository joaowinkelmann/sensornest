<template>
  <div class="q-pa-md">
    <h1 class="text-h4">{{ t('data.title') }}</h1>
    <p>{{ t('data.description') }}</p>
  </div>
</template>

<script setup>
import { useI18n } from 'vue-i18n';

const { t } = useI18n();
</script>
