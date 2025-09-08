<template>
  <q-page class="flex column no-wrap">
    <div class="col relative-position">
      <LeafletMap ref="mapRef" class="map-container" :zones="convertedZones" :active-zone-id="zonesStore.activeZoneId"
        :active-zone="zonesStore.activeZone" :center-lat="baseCoords.lat" :center-lng="baseCoords.lng"
        :mesh="meshesStore.selectedMesh"
        :devices="devicesStore.devices" :conversion-fn="convertDeviceToLatLng" :edit-mode="editMode"
        @vertex-added="handleVertexAdded" @vertex-removed="handleVertexRemoved" @vertex-moved="handleVertexMoved"
        @zone-updated="saveZone" @device-moved="handleDeviceMoved" @zone-clicked="handleZoneClicked"
        @draw-zone-clicked="handleDrawZoneClicked" />

      <FloatingSidebar ref="sidebarRef" class="floating-sidebar-position" :meshes="meshesStore.meshes"
        :zones="zonesStore.zones" :selected-mesh-id="meshesStore.selectedMeshId"
        :can-create-zone="!!meshesStore.selectedMeshId" :show-save-button="zonesStore.isZoneDirty"
        :show-undo-button="!!backupVertices" v-model:edit-mode="editMode"
        @mesh-select-clicked="showMeshDialog = true"
        @zone-manage-clicked="showZonesDialog = true" @zone-create-clicked="handleDrawZoneClicked"
        @save-zone-clicked="saveZone" @undo-zone-clicked="restoreZoneChanges" />
    </div>

    <MeshDialog :show="showMeshDialog" :meshes="meshesStore.meshes" :current-mesh-id="meshesStore.selectedMeshId"
      @close="showMeshDialog = false" @mesh-selected="onMeshSelected" />

    <ZonesDialog :show="showZonesDialog" :zones="zonesStore.zones" :active-zone-id="zonesStore.activeZoneId"
      @close="showZonesDialog = false" @zone-selected="onZoneSelected" />
  </q-page>
</template>

<script setup>
import { computed, ref, onMounted, watch } from 'vue';
import { useQuasar } from 'quasar';
import { useMeshesStore } from '@/stores/meshes';
import { useZonesStore } from '@/stores/zones';
import { useDevicesStore } from '@/stores/devices';
import { useRouter } from 'vue-router';

import LeafletMap from '@/components/LeafletMap.vue';
import FloatingSidebar from '@/components/FloatingSidebar.vue';
import MeshDialog from '@/components/MeshDialog.vue';
import ZonesDialog from '@/components/ZonesDialog.vue';

const $q = useQuasar();
const router = useRouter();
const meshesStore = useMeshesStore();
const zonesStore = useZonesStore();
const devicesStore = useDevicesStore();

const mapRef = ref(null);
const showMeshDialog = ref(false);
const showZonesDialog = ref(false);
const backupVertices = ref(null);
const editMode = ref('zones');

const SCALE = 0.0001; // For OpenStreetMap coordinate conversion

const isCustomMap = computed(() => !!meshesStore.selectedMesh?.mapUrl);

const baseCoords = computed(() => {
  const selectedMesh = meshesStore.selectedMesh;
  if (selectedMesh && !isCustomMap.value) {
    return { lat: selectedMesh.lat, lng: selectedMesh.lon };
  }
  return { lat: -23.5505, lng: -46.6333 }; // Default for OSM
});

onMounted(() => {
  meshesStore.fetchMeshes();
  if (!meshesStore.selectedMeshId) {
    $q.notify({
      type: 'info',
      message: 'Please select a mesh to start editing.',
    });
  }
});

watch(() => zonesStore.activeZoneId, (newZoneId) => {
  backupVertices.value = null;
  if (newZoneId) {
    devicesStore.fetchDevices(meshesStore.selectedMeshId);
  } else {
    devicesStore.clearDevices();
  }
});

const convertedZones = computed(() =>
  zonesStore.zones.map(zone => {
    if (!zone.vertices || zone.vertices.length === 0) {
      return { ...zone, vertices: [] };
    }
    if (isCustomMap.value) {
      return {
        ...zone,
        vertices: zone.vertices.map(v => ({ lat: v.y, lng: v.x })),
      };
    }
    return {
      ...zone,
      vertices: zone.vertices.map(v => ({
        lat: baseCoords.value.lat + v.y * SCALE,
        lng: baseCoords.value.lng + v.x * SCALE,
      })),
    };
  })
);

const convertToApiVertex = (latlng) => {
  if (isCustomMap.value) {
    return { x: Math.round(latlng.lng), y: Math.round(latlng.lat) };
  }
  return {
    x: Math.round((latlng.lng - baseCoords.value.lng) / SCALE),
    y: Math.round((latlng.lat - baseCoords.value.lat) / SCALE),
  };
};

const convertDeviceToLatLng = (device) => {
  if (isCustomMap.value) {
    return { lat: device.y, lng: device.x };
  }
  return {
    lat: baseCoords.value.lat + device.y * SCALE,
    lng: baseCoords.value.lng + device.x * SCALE,
  };
};

const pointToSegmentDistanceSq = (p, a, b) => {
  const vx = b.x - a.x;
  const vy = b.y - a.y;
  const wx = p.x - a.x;
  const wy = p.y - a.y;
  const segLenSq = vx * vx + vy * vy;
  if (segLenSq === 0) {
    const dx = p.x - a.x;
    const dy = p.y - a.y;
    return dx * dx + dy * dy;
  }
  let t = (wx * vx + wy * vy) / segLenSq;
  t = Math.max(0, Math.min(1, t));
  const projX = a.x + t * vx;
  const projY = a.y + t * vy;
  const dx = p.x - projX;
  const dy = p.y - projY;
  return dx * dx + dy * dy;
};

const findInsertionIndex = (vertices, newPoint) => {
  let bestIndex = vertices.length;
  let bestDist = Infinity;
  const n = vertices.length;
  if (n < 3) return n;
  for (let i = 0; i < n; i++) {
    const a = vertices[i];
    const b = vertices[(i + 1) % n];
    const distSq = pointToSegmentDistanceSq(newPoint, a, b);
    if (distSq < bestDist) {
      bestDist = distSq;
      bestIndex = i + 1;
    }
  }
  return bestIndex;
};

const handleVertexAdded = (latlng) => {
  const activeZone = zonesStore.activeZone;
  if (!activeZone) {
    $q.notify({ type: 'warning', message: 'Please select or create a zone first.' });
    return;
  }
  if (backupVertices.value === null) {
    backupVertices.value = JSON.parse(JSON.stringify(activeZone.vertices || []));
  }
  const newApiVertex = convertToApiVertex(latlng);
  const current = activeZone.vertices || [];
  const insertionIndex = findInsertionIndex(current, newApiVertex);
  const updatedVertices = [...current.slice(0, insertionIndex), newApiVertex, ...current.slice(insertionIndex)];
  zonesStore.updateLocalZone(activeZone.id, { vertices: updatedVertices });
};

const handleVertexRemoved = (vertexIndex) => {
  const activeZone = zonesStore.activeZone;
  if (!activeZone) return;
  if (backupVertices.value === null) {
    backupVertices.value = JSON.parse(JSON.stringify(activeZone.vertices || []));
  }
  const updatedVertices = activeZone.vertices.filter((_, idx) => idx !== vertexIndex);
  zonesStore.updateLocalZone(activeZone.id, { vertices: updatedVertices });
};

const handleVertexMoved = ({ index, latlng }) => {
  const activeZone = zonesStore.activeZone;
  if (!activeZone) return;
  if (backupVertices.value === null) {
    backupVertices.value = JSON.parse(JSON.stringify(activeZone.vertices || []));
  }
  const newApiVertex = convertToApiVertex(latlng);
  const updatedVertices = activeZone.vertices.map((v, idx) => (idx === index ? newApiVertex : v));
  zonesStore.updateLocalZone(activeZone.id, { vertices: updatedVertices });
};

const handleDeviceMoved = async ({ id, latlng }) => {
  const { x, y } = convertToApiVertex(latlng);
  try {
    await devicesStore.updateDevice(id, { x, y });
    $q.notify({
      type: 'positive',
      message: 'Device position updated successfully!',
      timeout: 2000
    });
  } catch (error) {
    console.error('Failed to update device position:', error);
    $q.notify({
      type: 'negative',
      message: 'Failed to update device position.',
    });
    devicesStore.fetchDevices(meshesStore.selectedMeshId);
  }
};

const saveZone = async () => {
  const zone = zonesStore.activeZone;
  if (!zone) {
    $q.notify({ type: 'warning', message: 'No active zone to save.' });
    return;
  }
  try {
    await zonesStore.updateZone(zone.id, { name: zone.name, vertices: zone.vertices, meshId: zone.meshId });
    mapRef.value?.setMode(null);
    backupVertices.value = null;
    zonesStore.isZoneDirty = false;
    $q.notify({ type: 'positive', message: 'Zone saved successfully!' });
  } catch (error) {
    console.error('Failed to save zone:', error);
    $q.notify({ type: 'negative', message: 'Failed to save zone. Please try again.' });
  }
};

const onMeshSelected = (meshId) => {
  meshesStore.selectMesh(meshId);
  showMeshDialog.value = false;
  $q.notify({ type: 'positive', message: 'Mesh selected successfully!' });
};

const onZoneSelected = (zoneId) => {
  handleZoneClicked(zoneId, true);
  showZonesDialog.value = false;
};

const handleZoneClicked = (zoneId, force = false) => {
  if (zonesStore.activeZoneId === zoneId) return;

  const switchZone = () => {
    restoreZoneChanges(); // Discard any unsaved changes silently
    zonesStore.setActiveZone(zoneId);
    if (!force) {
      $q.notify({ type: 'info', message: `Zone activated. You can now view its details.` });
    }
  };

  if (zonesStore.isZoneDirty && !force) {
    $q.dialog({
      title: 'Confirm',
      message: 'You have unsaved changes in the current zone. Are you sure you want to discard them and switch to another zone?',
      cancel: true,
      persistent: true,
      ok: { label: 'Discard & Switch', color: 'negative' }
    }).onOk(() => {
      switchZone();
    });
  } else {
    switchZone();
  }
};

const handleDrawZoneClicked = () => {
  if (!meshesStore.selectedMeshId) {
    $q.notify({ type: 'warning', message: 'Please select a mesh first.' });
    return;
  }
  $q.dialog({
    title: 'Create New Zone',
    message: 'Enter a name for the new zone:',
    prompt: { model: '', type: 'text', isValid: val => val && val.length > 0 },
    persistent: true,
    ok: { label: 'Create', color: 'primary' },
    cancel: { label: 'Cancel', color: 'grey-7', flat: true }
  }).onOk(async (name) => {
    if (name.trim()) {
      try {
        const newZoneData = {
          name: name.trim(),
          vertices: [],
          meshId: meshesStore.selectedMeshId
        };
        const newZone = await zonesStore.createZone(newZoneData);
        $q.notify({ type: 'positive', message: `Zone "${name}" created! Click on the map to start adding vertices.` });
        if (newZone && newZone.id) {
          zonesStore.setActiveZone(newZone.id);
          mapRef.value?.setMode('add'); // Switch to add mode
        }
      } catch (error) {
        console.error('Failed to create zone:', error);
        $q.notify({ type: 'negative', message: 'Failed to create zone. Please try again.' });
      }
    }
  });
};

const restoreZoneChanges = () => {
  const zone = zonesStore.activeZone;
  if (!zone || !backupVertices.value) return;
  zonesStore.updateLocalZone(zone.id, { vertices: backupVertices.value });
  zonesStore.isZoneDirty = false;
  backupVertices.value = null;
  try {
    mapRef.value?.focusOnZone(zone.id);
  } catch (err) {
    console.warn('Failed to focus on zone after undo:', err);
  }
  $q.notify({ type: 'info', message: 'Undid unsaved changes.' });
};
</script>

<style scoped>
.map-container {
  position: absolute;
  top: 0;
  left: 0;
  width: 100%;
  height: 100%;
}

.floating-sidebar-position {
  position: absolute;
  top: 20px;
  right: 20px;
  z-index: 1000;
}
</style>
