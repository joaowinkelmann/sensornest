<template>
  <q-page class="flex column q-pa-md">
    <h1 class="text-h4">{{ t('reports.title') }}</h1>
    <p>{{ t('reports.description') }}</p>
  </q-page>
</template>

<script setup>
import { useI18n } from 'vue-i18n';

const { t } = useI18n();
</script>
