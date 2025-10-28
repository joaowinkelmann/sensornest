<template>
  <q-page class="flex column no-wrap">
    <div class="col relative-position">
      <LeafletMap ref="mapRef" class="map-container" :zones="convertedZones" :active-zone-id="zonesStore.activeZoneId"
        :active-zone="zonesStore.activeZone" :center-lat="baseCoords.lat" :center-lng="baseCoords.lng"
        :mesh="meshesStore.selectedMesh"
        :devices="devicesStore.devices" :conversion-fn="convertDeviceToLatLng" :edit-mode="editMode"
        @vertex-added="handleVertexAdded" @vertex-removed="handleVertexRemoved" @vertex-moved="handleVertexMoved"
        @zone-updated="saveZone" @device-moved="handleDeviceMoved"
        @draw-zone-clicked="handleDrawZoneClicked"
        @save-new-zone="handleSaveNewZone"
        @cancel-new-zone="handleCancelNewZone" />

      <FloatingSidebar ref="sidebarRef" class="floating-sidebar-position" :meshes="meshesStore.meshes"
        :zones="zonesStore.zones" :selected-mesh-id="meshesStore.selectedMeshId"
        :active-zone-id="zonesStore.activeZoneId"
        :is-zone-dirty="zonesStore.isZoneDirty"
        :can-create-zone="!!meshesStore.selectedMeshId" :show-save-button="zonesStore.isZoneDirty"
        :show-undo-button="zonesStore.backupVertices !== null" v-model:edit-mode="editMode"
        @mesh-select-clicked="handleMeshSelected" @zone-create-clicked="handleDrawZoneClicked"
        @save-zone-clicked="saveZone" @undo-zone-clicked="zonesStore.restoreActiveZone()" />
    </div>
  </q-page>
</template>

<script setup>
import { computed, ref, onMounted, watch } from 'vue';
import { useQuasar } from 'quasar';
import { useMeshesStore } from '@/stores/meshes';
import { useZonesStore } from '@/stores/zones';
import { useDevicesStore } from '@/stores/devices';
import { useI18n } from 'vue-i18n';

import LeafletMap from '@/components/LeafletMap.vue';
import FloatingSidebar from '@/components/FloatingSidebar.vue';
import MeshDialog from '@/components/MeshDialog.vue';

const { t } = useI18n();
const $q = useQuasar();
const meshesStore = useMeshesStore();
const zonesStore = useZonesStore();
const devicesStore = useDevicesStore();

const mapRef = ref(null);
const showMeshDialog = ref(false);
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

onMounted(async () => {
  await meshesStore.fetchMeshes();
  
  // Auto-select first mesh if none is selected
  if (!meshesStore.selectedMeshId && meshesStore.meshes.length > 0) {
    meshesStore.selectMesh(meshesStore.meshes[0].id);
  }
  
  // Only show notification if there are no meshes at all
  if (meshesStore.meshes.length === 0) {
    $q.notify({
      type: 'info',
      message: t('notifications.noMeshesAvailable'),
    });
  }
});

watch(() => zonesStore.activeZoneId, (newZoneId) => {
  zonesStore.backupVertices = null;
  // Removed duplicate fetch; zones store now handles fetching on change
  if (!newZoneId) {
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
    $q.notify({ type: 'warning', message: t('notifications.zonePleaseSelect') });
    return;
  }
  if (zonesStore.backupVertices === null) {
    zonesStore.backupActiveZone();
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
  if (zonesStore.backupVertices === null) {
    zonesStore.backupActiveZone();
  }
  const updatedVertices = activeZone.vertices.filter((_, idx) => idx !== vertexIndex);
  zonesStore.updateLocalZone(activeZone.id, { vertices: updatedVertices });
};

const handleVertexMoved = ({ index, latlng }) => {
  const activeZone = zonesStore.activeZone;
  if (!activeZone) return;
  if (zonesStore.backupVertices === null) {
    zonesStore.backupActiveZone();
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
      message: t('notifications.deviceMoved'),
      timeout: 2000
    });
  } catch (error) {
    console.error('Failed to update device position:', error);
    $q.notify({
      type: 'negative',
      message: t('notifications.deviceMovedError'),
    });
    devicesStore.fetchDevices(meshesStore.selectedMeshId);
  }
};

const saveZone = async () => {
  const zone = zonesStore.activeZone;
  if (!zone) {
    $q.notify({ type: 'warning', message: t('notifications.noActiveZone') });
    return;
  }
  try {
    await zonesStore.updateZone(zone.id, { name: zone.name, vertices: zone.vertices, meshId: zone.meshId });
    mapRef.value?.setMode(null);
    zonesStore.backupVertices = null;
    zonesStore.isZoneDirty = false;
    $q.notify({ type: 'positive', message: t('notifications.zoneSaved') });
  } catch (error) {
    console.error('Failed to save zone:', error);
    $q.notify({ type: 'negative', message: t('notifications.zoneSavedError') });
  }
};

const handleMeshSelected = (meshId) => {
  if (meshId === meshesStore.selectedMeshId) return;
  
  const switchMesh = () => {
    if (zonesStore.isZoneDirty) {
      zonesStore.restoreActiveZone();
    }
    meshesStore.selectMesh(meshId);
    $q.notify({ type: 'positive', message: t('notifications.meshSelected') });
  };

  if (zonesStore.isZoneDirty) {
    $q.dialog({
      title: t('mapsidebar.confirmTitle'),
      message: t('mapsidebar.unsavedChanges'),
      cancel: true,
      persistent: true,
      ok: { label: t('mapsidebar.discardButton'), color: 'negative' }
    }).onOk(() => {
      switchMesh();
    });
  } else {
    switchMesh();
  }
};

const handleDrawZoneClicked = () => {
  if (!meshesStore.selectedMeshId) {
    $q.notify({ type: 'warning', message: t('notifications.selectMeshFirst') });
    return;
  }
  mapRef.value?.setMode('draw-zone');
};

const handleCancelNewZone = () => {
  mapRef.value?.setMode(null);
};

const handleSaveNewZone = (vertices) => {
  $q.dialog({
    title: t('mapEditor.createZoneTitle'),
    message: t('mapEditor.createZoneMessage'),
    prompt: { model: '', type: 'text', isValid: val => val && val.length > 0 },
    persistent: true,
    ok: { label: t('mapEditor.createButton'), color: 'primary' },
    cancel: { label: t('mapEditor.cancelButton'), color: 'grey-7', flat: true }
  }).onOk(async (name) => {
    if (name.trim()) {
      try {
        const newZoneData = {
          name: name.trim(),
          vertices: vertices.map(convertToApiVertex),
          meshId: meshesStore.selectedMeshId
        };
        const newZone = await zonesStore.createZone(newZoneData);
        $q.notify({ type: 'positive', message: t('notifications.zoneCreated') });
        if (newZone && newZone.id) {
          zonesStore.setActiveZone(newZone.id);
          mapRef.value?.setMode(null);
        }
      } catch (error) {
        console.error('Failed to create zone:', error);
        $q.notify({ type: 'negative', message: t('notifications.zoneCreatedError') });
      }
    }
  });
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
  top: 96px;
  right: 20px;
  z-index: 1000;
}
</style>
