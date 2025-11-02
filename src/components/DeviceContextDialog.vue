<template>
  <q-dialog :model-value="show" @update:model-value="$emit('close')">
    <q-card style="min-width: 500px">
      <q-card-section class="text-h6">
        {{ t('devices.contextDialog.title') }}
      </q-card-section>

      <q-tabs v-model="tab" class="text-grey" active-color="primary" indicator-color="primary" align="justify" narrow-indicator>
        <q-tab name="stats" :label="t('devices.contextDialog.statisticsTab')" />
        <q-tab name="zone" :label="t('devices.contextDialog.zoneTab')" />
      </q-tabs>

      <q-separator />

      <q-tab-panels v-model="tab" animated>
        <q-tab-panel name="stats">
          <div v-if="loading" class="text-center">
            <q-spinner-dots size="50px" />
          </div>
          <div v-else-if="error" class="text-negative">
            {{ t('devices.statisticsError') }}
          </div>
          <div v-else-if="statistics">
            <q-btn-toggle
              v-model="chartPeriod"
              :options="[
                { label: t('devices.timeframes.day'), value: 'day' },
                { label: t('devices.timeframes.week'), value: 'week' },
                { label: t('devices.timeframes.month'), value: 'month' },
              ]"
            />
            <StatisticsChart :chart-data="statistics" :period="chartPeriod" />
          </div>
        </q-tab-panel>

        <q-tab-panel name="zone">
          <DeviceZoneDialog
            :show="show && tab === 'zone'"
            :device="device"
            @zone-changed="handleZoneChanged"
          />
        </q-tab-panel>
      </q-tab-panels>

      <q-card-actions align="right">
        <q-btn flat :label="t('buttons.close')" color="primary" @click="$emit('close')" />
      </q-card-actions>
    </q-card>
  </q-dialog>
</template>

<script setup>
import { ref, watch } from 'vue';
import { useI18n } from 'vue-i18n';
import { useStatistics } from '@/composables/useStatistics';
import StatisticsChart from './StatisticsChart.vue';
import DeviceZoneDialog from './DeviceZoneDialog.vue';
import { useQuasar } from 'quasar';
import { useDevicesStore } from '@/stores/devices';

const props = defineProps({
  show: { type: Boolean, required: true },
  device: { type: Object, required: true }
});

const emit = defineEmits(['close']);
const $q = useQuasar();
const devicesStore = useDevicesStore();

const { t } = useI18n();
const { statistics, loading, error, fetchDeviceGraph } = useStatistics();

const tab = ref('stats');
const chartPeriod = ref('day');

watch(() => props.show, (newVal) => {
  if (newVal && props.device) {
    fetchDeviceGraph(props.device.id);
  }
});

const handleZoneChanged = async (newZoneId) => {
  try {
    await devicesStore.updateDevice(props.device.id, { zoneId: newZoneId });
    $q.notify({ type: 'positive', message: 'Device zone updated' });
    emit('close');
  } catch (e) {
    console.warn('Failed to update device zone.', e);
    $q.notify({ type: 'negative', message: 'Failed to update device zone' });
  }
};
</script>
