<template>
  <div>
    <div class="text-h6 q-mt-md">Zone Statistics</div>
    <q-list bordered separator>
      <q-item v-for="zone in zonesStore.zones.filter(z => z.meshId === meshId)" :key="zone.id">
        <q-item-section>
          <q-item-label>{{ zone.name }}</q-item-label>
        </q-item-section>
        <q-item-section side>
          <q-btn flat round icon="bar_chart" @click="showStatistics(zone)" aria-label="Show statistics" />
        </q-item-section>
      </q-item>
    </q-list>

    <q-dialog v-model="showStatsDialog">
      <q-card>
        <q-card-section>
          <div class="text-h6">Statistics for {{ selectedZone?.name }}</div>
        </q-card-section>

        <q-card-section v-if="loading">
          <q-spinner-dots size="50px" />
        </q-card-section>

        <q-card-section v-if="error">
          <p class="text-negative">Could not load statistics.</p>
        </q-card-section>

        <q-card-section v-if="statistics">
          <pre>{{ statistics }}</pre>
        </q-card-section>

        <q-card-actions align="right">
          <q-btn flat label="Close" color="primary" v-close-popup />
        </q-card-actions>
      </q-card>
    </q-dialog>
  </div>
</template>

<script setup>
import { ref, onMounted } from 'vue';
import { useZonesStore } from '@/stores/zones';
import { useStatistics } from '@/composables/useStatistics';

const props = defineProps({
  meshId: {
    type: String,
    required: true,
  },
});

const zonesStore = useZonesStore();
const { statistics, loading, error, fetchStatistics } = useStatistics();

const showStatsDialog = ref(false);
const selectedZone = ref(null);

onMounted(() => {
  zonesStore.fetchAllZones();
});

const showStatistics = async (zone) => {
  selectedZone.value = zone;
  await fetchStatistics('zone', zone.id);
  showStatsDialog.value = true;
};
</script>
