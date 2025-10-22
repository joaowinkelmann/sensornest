<template>
  <q-layout view="hHh lpR fFf">
    <q-header class="bg-primary text-white">
      <q-toolbar>
        <q-toolbar-title>SensorNest</q-toolbar-title>
        <q-space />
        <q-btn
          dense
          round
          icon="translate"
          class="q-mr-sm"
        >
          <q-menu anchor="top right" self="top right">
            <q-list>
              <q-item
                v-for="option in localeOptions"
                :key="option.value"
                clickable
                v-close-popup
                @click="locale = option.value"
              >
                <q-item-section>{{ option.label }}</q-item-section>
              </q-item>
            </q-list>
          </q-menu>
        </q-btn>
      </q-toolbar>
    </q-header>

    <q-page-container>
      <router-view />
    </q-page-container>
  </q-layout>
</template>

<script setup>
import { useI18n } from "vue-i18n";

const { locale } = useI18n();
const localeOptions = [
  { value: "en-US", label: "English" },
  { value: "es-ES", label: "Español" },
  { value: "pt-BR", label: "Português (BR)" },
];
</script>