<template>
  <div>
    <div class="row items-center justify-between q-mb-md">
      <div class="text-h4">Device Manager</div>
    </div>

    <!-- Filters -->
    <div class="row q-gutter-md q-mb-md">
      <q-select
        v-model="selectedMesh"
        :options="meshOptions"
        label="Filter by Mesh"
        emit-value
        map-options
        clearable
        @update:model-value="onMeshChange"
        style="min-width: 200px;"
      />
      <q-select
        v-model="selectedZone"
        :options="zoneOptions"
        label="Filter by Zone"
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
        <q-item-section>
          <q-item-label>{{ device.name }}</q-item-label>
          <q-item-label caption>{{ device.macAddress }}</q-item-label>
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
          <div class="text-h6">Statistics for {{ selectedDevice?.name }}</div>
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
import { ref, onMounted, computed } from 'vue';
import { useDevicesStore } from '@/stores/devices';
import { useMeshesStore } from '@/stores/meshes';
import { useZonesStore } from '@/stores/zones';
import { useStatistics } from '@/composables/useStatistics';
import DeviceDialog from './DeviceDialog.vue';
import { useQuasar } from 'quasar';

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
  if (!selectedMesh.value) {
    return zonesStore.zones.map(zone => ({ label: zone.name, value: zone.id }));
  }
  return zonesStore.zones
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
    message: 'Device updated successfully!',
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
