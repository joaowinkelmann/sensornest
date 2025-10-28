<template>
  <div class="q-pa-md">
    <h1 class="text-h4">{{ t('reports.title') }}</h1>
    <p>{{ t('reports.description') }}</p>
  </div>
</template>

<script setup>
import { useI18n } from 'vue-i18n';

const { t } = useI18n();
</script>
