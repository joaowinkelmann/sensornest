<template>
  <div>
    <div class="row items-center justify-between q-mb-md">
      <div class="text-h4">{{ t('devices.title') }}</div>
    </div>

    <!-- Filters -->
    <div class="row q-gutter-md q-mb-md">
      <q-select
        v-model="selectedMesh"
        :options="meshOptions"
        :label="t('devices.filterByMesh')"
        emit-value
        map-options
        clearable
        @update:model-value="onMeshChange"
        style="min-width: 200px;"
      />
      <q-select
        v-model="selectedZone"
        :options="zoneOptions"
        :label="t('devices.filterByZone')"
        emit-value
        map-options
        clearable
        @update:model-value="onZoneChange"
        style="min-width: 200px;"
        :disable="!selectedMesh"
      />
    </div>

    <!-- Device List -->
    <q-list bordered separator>
      <q-item v-for="device in devicesStore.devices" :key="device.id">
        <q-item-section style="width: 100%;">
          <div class="device-row">
            <div class="device-info">
              <div class="device-name">{{ device.name }}</div>
              <div class="device-mac caption">{{ device.macAddress }}</div>
            </div>

            <div
              class="color-preview"
              :title="device.deviceColor ? device.deviceColor : t('devices.noColor')"
            >
              <div
                v-if="device.deviceColor"
                class="color-dot"
                :style="{ backgroundColor: device.deviceColor }"
              />
              <q-icon v-else name="help_outline" size="18px" />
            </div>
          </div>
        </q-item-section>
        <q-item-section side>
          <q-btn flat round icon="edit" @click="openEditDialog(device)" />
          <q-btn flat round icon="bar_chart" @click="showStatistics(device)" />
        </q-item-section>
      </q-item>
    </q-list>

    <DeviceDialog
      :show="showDialog"
      :device-to-edit="editingDevice"
      @close="showDialog = false"
      @device-updated="handleDeviceUpdated"
    />

    <q-dialog v-model="showStatsDialog">
      <q-card>
        <q-card-section>
          <div class="text-h6">{{ t('devices.statisticsTitle', { deviceName: selectedDevice?.name }) }}</div>
        </q-card-section>

        <q-card-section v-if="loading">
          <q-spinner-dots size="50px" />
        </q-card-section>

        <q-card-section v-if="error">
          <p class="text-negative">{{ t('devices.statisticsError') }}</p>
        </q-card-section>

        <q-card-section v-if="statistics">
          <pre>{{ statistics }}</pre>
        </q-card-section>

        <q-card-actions align="right">
          <q-btn flat :label="t('devices.closeButton')" color="primary" v-close-popup />
        </q-card-actions>
      </q-card>
    </q-dialog>
  </div>
</template>

<script setup>
import { ref, onMounted, computed } from 'vue';
import { useDevicesStore } from '@/stores/devices';
import { useMeshesStore } from '@/stores/meshes';
import { useZonesStore } from '@/stores/zones';
import { useStatistics } from '@/composables/useStatistics';
import DeviceDialog from './DeviceDialog.vue';
import { useQuasar } from 'quasar';
import { useI18n } from 'vue-i18n';

const { t } = useI18n();
const devicesStore = useDevicesStore();
const meshesStore = useMeshesStore();
const zonesStore = useZonesStore();
const { statistics, loading, error, fetchStatistics } = useStatistics();
const $q = useQuasar();

const selectedMesh = ref(null);
const selectedZone = ref(null);
const showDialog = ref(false);
const editingDevice = ref(null);
const showStatsDialog = ref(false);
const selectedDevice = ref(null);

onMounted(async () => {
  await meshesStore.fetchMeshes();
  await zonesStore.fetchAllZones();
  await devicesStore.fetchDevices();
});

const meshOptions = computed(() =>
  meshesStore.meshes.map(mesh => ({ label: mesh.name, value: mesh.id }))
);

const zoneOptions = computed(() => {
  // Prefer cross-mesh list, but be defensive for tests/mocks
  const source =
    (Array.isArray(zonesStore.allZones) && zonesStore.allZones.length >= 0
      ? zonesStore.allZones
      : []) // ensure array
    .length ? zonesStore.allZones
    : (Array.isArray(zonesStore.zones) ? zonesStore.zones : []);

  if (!selectedMesh.value) {
    return source.map(zone => ({ label: zone.name, value: zone.id }));
  }
  return source
    .filter(zone => zone.meshId === selectedMesh.value)
    .map(zone => ({ label: zone.name, value: zone.id }));
});

const onMeshChange = (meshId) => {
  selectedZone.value = null; // Reset zone filter
  devicesStore.fetchDevices({ meshId: meshId });
};

const onZoneChange = (zoneId) => {
  devicesStore.fetchDevices({ zoneId: zoneId });
};

const openEditDialog = (device) => {
  editingDevice.value = { ...device };
  showDialog.value = true;
};

const showStatistics = async (device) => {
  selectedDevice.value = device;
  await fetchStatistics('device', device.id);
  showStatsDialog.value = true;
};

const handleDeviceUpdated = () => {
  showDialog.value = false;
  editingDevice.value = null;
  $q.notify({
    type: 'positive',
    message: t('devices.updateSuccess'),
  });
  // Refetch devices to show updated data
  if (selectedZone.value) {
    devicesStore.fetchDevices({ zoneId: selectedZone.value });
  } else if (selectedMesh.value) {
    devicesStore.fetchDevices({ meshId: selectedMesh.value });
  } else {
    devicesStore.fetchDevices();
  }
};
</script>

<style scoped>
.device-row {
  display: flex;
  align-items: center;
  justify-content: space-between;
  width: 100%;
}

.device-info {
  display: flex;
  flex-direction: column;
  min-width: 0;
}

.device-name {
  font-weight: 600;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.device-mac.caption {
  font-size: 0.85rem;
  opacity: 0.75;
}

.color-preview {
  display: flex;
  align-items: center;
  justify-content: center;
  width: 36px;
  height: 36px;
  margin-left: 12px;
}

.color-dot {
  width: 16px;
  height: 16px;
  border-radius: 50%;
  border: 2px solid #fff;
  box-shadow: 0 1px 3px rgba(0,0,0,0.12);
}
</style>
