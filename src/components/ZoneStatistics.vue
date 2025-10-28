<template>
  <div class="q-pa-md">
    <q-card>
      <q-card-section>
        <div class="text-h6">{{ t('zoneStatistics.title') }}</div>
        <div class="text-subtitle2">{{ t('zoneStatistics.subtitle') }}</div>
      </q-card-section>

      <q-card-section>
        <q-list bordered separator>
          <q-item v-for="zone in zonesStore.zones" :key="zone.id">
            <q-item-section>
              <q-item-label>{{ zone.name }}</q-item-label>
            </q-item-section>
            <q-item-section side>
              <q-btn
                flat
                round
                icon="bar_chart"
                :aria-label="t('zoneStatistics.showStatistics')"
                @click="loadStatistics('zone', zone.id)"
              />
            </q-item-section>
          </q-item>
        </q-list>
      </q-card-section>

      <q-card-section v-if="loading">
        <q-spinner-dots color="primary" size="40px" />
        <p>{{ t('zoneStatistics.loading') }}</p>
      </q-card-section>

      <q-card-section v-if="error">
        <p class="text-negative">{{ error }}</p>
      </q-card-section>

      <q-card-section v-if="statistics">
        <div class="text-h6">{{ t('zoneStatistics.results') }}</div>
        <!-- Render your statistics here -->
        <pre>{{ statistics }}</pre>
      </q-card-section>
    </q-card>
  </div>
</template>

<script setup>
import { onMounted } from 'vue';
import { useZonesStore } from '@/stores/zones';
import { useStatistics } from '@/composables/useStatistics';
import { useI18n } from 'vue-i18n';

const { t } = useI18n();
defineProps({
  meshId: {
    type: String,
    required: true,
  },
});

const zonesStore = useZonesStore();
const { statistics, loading, error, fetchStatistics } = useStatistics();

onMounted(() => {
  zonesStore.fetchAllZones();
});

const loadStatistics = (type, id) => {
  fetchStatistics(type, id);
};
</script>
