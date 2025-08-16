<template>
  <div class="dashboard-root">
    <!-- Map Fullscreen -->
    <div class="map-fullscreen">
      <LeafletMap ref="mapRef" :zones="convertedZones" :active-zone-id="zonesStore.activeZoneId"
        :active-zone="zonesStore.activeZone" :center-lat="baseCoords.lat" :center-lng="baseCoords.lng"
        @vertex-added="handleVertexAdded" @vertex-removed="handleVertexRemoved" @vertex-moved="handleVertexMoved"
        @zone-updated="saveZone" />
    </div>

    <!-- Floating Sidebar Component -->
    <FloatingSidebar ref="sidebarRef" :meshes="meshesStore.meshes" :zones="zonesStore.zones"
      :selected-mesh-id="meshesStore.selectedMeshId" :can-create-zone="!!meshesStore.selectedMeshId"
      :show-save-button="zonesStore.isZoneDirty" :show-undo-button="!!backupVertices"
      @mesh-select-clicked="showMeshDialog = true" @mesh-create-clicked="promptCreateMesh"
      @zone-manage-clicked="showZonesDialog = true" @zone-create-clicked="promptCreateZone"
      @save-zone-clicked="saveZone" @undo-zone-clicked="restoreZoneChanges" @logout-clicked="handleLogout" />

    <!-- Dialog Components -->
    <MeshDialog :show="showMeshDialog" :meshes="meshesStore.meshes" :current-mesh-id="meshesStore.selectedMeshId"
      @close="showMeshDialog = false" @mesh-selected="onMeshSelected" />

    <ZonesDialog :show="showZonesDialog" :zones="zonesStore.zones" :active-zone-id="zonesStore.activeZoneId"
      @close="showZonesDialog = false" @zone-selected="onZoneSelected" />

    <!-- Initial Setup Modals -->
    <MeshManagerModal v-if="!meshesStore.selectedMeshId && authStore.isAuthenticated"
      :show="!meshesStore.selectedMeshId && authStore.isAuthenticated" @mesh-selected="onMeshSelected" />

    <!-- Auth Modals -->
    <LoginModal v-if="!authStore.isAuthenticated && !showRegisterModal"
      :show="!authStore.isAuthenticated && !showRegisterModal" @show-register="showRegisterModal = true" />
    <RegisterModal v-if="showRegisterModal" :show="showRegisterModal" @show-login="showRegisterModal = false" />
  </div>
</template>

<script setup>
import { computed, ref, onMounted, watch } from 'vue';
import { useQuasar } from 'quasar';
import { useAuthStore } from '@/stores/auth';
import { useMeshesStore } from '@/stores/meshes';
import { useZonesStore } from '@/stores/zones';

// Map component
import LeafletMap from '@/components/LeafletMap.vue';

// Floating sidebar components
import FloatingSidebar from '@/components/FloatingSidebar.vue';
import MeshDialog from '@/components/MeshDialog.vue';
import ZonesDialog from '@/components/ZonesDialog.vue';

// Modal components
import LoginModal from '@/components/LoginModal.vue';
import RegisterModal from '@/components/RegisterModal.vue';
import MeshManagerModal from '@/components/MeshManagerModal.vue';

const $q = useQuasar();
const authStore = useAuthStore();
const meshesStore = useMeshesStore();
const zonesStore = useZonesStore();

// Component refs
const sidebarRef = ref(null);
const mapRef = ref(null);

// Modal states
const showRegisterModal = ref(false);
const showMeshDialog = ref(false);
const showZonesDialog = ref(false);

// --- new: backup for undoing local edits ---
const backupVertices = ref(null);

// Map configuration
const SCALE = 0.0001;

const baseCoords = computed(() => {
  const selectedMesh = meshesStore.selectedMesh;
  if (selectedMesh && selectedMesh.lat && selectedMesh.lon) {
    return { lat: selectedMesh.lat, lng: selectedMesh.lon };
  }
  // Fallback
  return { lat: -23.5505, lng: -46.6333 };
});

onMounted(() => {
  if (authStore.isAuthenticated) {
    meshesStore.fetchMeshes();
  }
});

// When active zone changes, drop any pending backup (new selection)
watch(() => zonesStore.activeZoneId, () => {
  backupVertices.value = null;
});

// Computed properties for map data conversion
const convertedZones = computed(() =>
  zonesStore.zones.map(zone => {
    if (!zone.vertices || zone.vertices.length === 0) {
      return { ...zone, vertices: [] };
    }
    const xs = zone.vertices.map(v => v.x);
    const ys = zone.vertices.map(v => v.y);
    const centroidX = xs.reduce((a, b) => a + b, 0) / xs.length;
    const centroidY = ys.reduce((a, b) => a + b, 0) / ys.length;

    return {
      ...zone,
      vertices: zone.vertices.map(v => ({
        lat: baseCoords.value.lat + (v.y - centroidY) * SCALE,
        lng: baseCoords.value.lng + (v.x - centroidX) * SCALE,
      })),
    };
  })
);

// Coordinate conversion utilities
const convertToApiVertex = (latlng) => {
  const activeZone = zonesStore.activeZone;
  if (!activeZone || activeZone.vertices.length === 0) {
    return {
      x: Math.round((latlng.lng - baseCoords.value.lng) / SCALE),
      y: Math.round((latlng.lat - baseCoords.value.lat) / SCALE),
    };
  }

  const xs = activeZone.vertices.map(v => v.x);
  const ys = activeZone.vertices.map(v => v.y);
  const centroidX = xs.reduce((a, b) => a + b, 0) / xs.length;
  const centroidY = ys.reduce((a, b) => a + b, 0) / ys.length;

  return {
    x: Math.round(((latlng.lng - baseCoords.value.lng) / SCALE) + centroidX),
    y: Math.round(((latlng.lat - baseCoords.value.lat) / SCALE) + centroidY),
  };
};

// Map event handlers
const handleVertexAdded = (latlng) => {
  const activeZone = zonesStore.activeZone;
  if (!activeZone) {
    $q.notify({
      type: 'warning',
      message: 'Please select or create a zone first.',
      position: 'top'
    });
    return;
  }

  // create backup before the first local modification
  if (backupVertices.value === null) {
    backupVertices.value = JSON.parse(JSON.stringify(activeZone.vertices || []));
  }

  const newApiVertex = convertToApiVertex(latlng);
  const updatedVertices = [...activeZone.vertices, newApiVertex];
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

// Action handlers
const saveZone = async () => {
  const zone = zonesStore.activeZone;
  if (!zone) {
    $q.notify({
      type: 'warning',
      message: 'No active zone to save.',
      position: 'top'
    });
    return;
  }

  try {
    await zonesStore.updateZone(zone.id, {
      name: zone.name,
      vertices: zone.vertices,
      meshId: zone.meshId,
    });
    // Quando salvar, volta o modo do mapa
    mapRef.value?.setMode(null);

    // clear backup after successful save
    backupVertices.value = null;
    zonesStore.isZoneDirty = false;

    $q.notify({
      type: 'positive',
      message: 'Zone saved successfully!',
      position: 'top'
    });
  } catch (error) {
    console.error('Failed to save zone:', error);
    $q.notify({
      type: 'negative',
      message: 'Failed to save zone. Please try again.',
      position: 'top'
    });
  }
};

const onMeshSelected = (meshId) => {
  meshesStore.selectMesh(meshId);
  showMeshDialog.value = false;
  $q.notify({
    type: 'positive',
    message: 'Mesh selected successfully!',
    position: 'top'
  });
};

const onZoneSelected = (zoneId) => {
  zonesStore.setActiveZone(zoneId);
  showZonesDialog.value = false;
  $q.notify({
    type: 'info',
    message: 'Zone selected. You can now edit it on the map.',
    position: 'top'
  });
};

const promptCreateMesh = () => {
  $q.dialog({
    title: 'Create New Mesh',
    message: 'Enter a name for the new mesh:',
    prompt: {
      model: '',
      type: 'text',
      isValid: val => val && val.length > 0
    },
    // cancel: true,
    persistent: true,
    ok: {
      label: 'Create',
      color: 'primary'
    },
    cancel: {
      label: 'Cancel',
      color: 'grey-7',
      flat: true
    }
  }).onOk(async (name) => {
    if (name.trim()) {
      try {
        await meshesStore.createMesh(name.trim());
        $q.notify({
          type: 'positive',
          message: `Mesh "${name}" created successfully!`,
          position: 'top'
        });
      } catch (error) {
        console.error('Failed to create mesh:', error);
        $q.notify({
          type: 'negative',
          message: 'Failed to create mesh. Please try again.',
          position: 'top'
        });
      }
    }
  });
};

const promptCreateZone = () => {
  if (!meshesStore.selectedMeshId) {
    $q.notify({
      type: 'warning',
      message: 'Please select a mesh first.',
      position: 'top'
    });
    return;
  }

  $q.dialog({
    title: 'Create New Zone',
    message: 'Enter a name for the new zone:',
    prompt: {
      model: '',
      type: 'text',
      isValid: val => val && val.length > 0
    },
    // cancel: true,
    persistent: true,
    ok: {
      label: 'Create',
      color: 'primary'
    },
    cancel: {
      label: 'Cancel',
      color: 'grey-7',
      flat: true
    }
  }).onOk(async (name) => {
    if (name.trim()) {
      try {
        const newZone = await zonesStore.createZone(meshesStore.selectedMeshId, {
          name: name.trim(),
          vertices: []
        });
        $q.notify({
          type: 'positive',
          message: `Zone "${name}" created! You can now add points on the map.`,
          position: 'top',
          timeout: 4000
        });
        // Auto-select the new zone
        if (newZone && newZone.id) {
          zonesStore.setActiveZone(newZone.id);
        }
      } catch (error) {
        console.error('Failed to create zone:', error);
        $q.notify({
          type: 'negative',
          message: 'Failed to create zone. Please try again.',
          position: 'top'
        });
      }
    }
  });
};

const handleLogout = () => {
  $q.dialog({
    title: 'Confirm Logout',
    message: 'Are you sure you want to logout?',
    // cancel: true,
    persistent: true,
    ok: {
      label: 'Logout',
      color: 'negative'
    },
    cancel: {
      label: 'Cancel',
      color: 'grey-7',
      flat: true
    }
  }).onOk(() => {
    authStore.logout();
    $q.notify({
      type: 'info',
      message: 'Logged out successfully',
      position: 'top'
    });
  });
};

// Restore previous vertices and fit view
const restoreZoneChanges = () => {
  const zone = zonesStore.activeZone;
  if (!zone || !backupVertices.value) return;

  zonesStore.updateLocalZone(zone.id, { vertices: backupVertices.value });
  zonesStore.isZoneDirty = false;
  backupVertices.value = null;

  // Fit the map view to the zone
  try {
    mapRef.value?.focusOnZone(zone.id); // uses LeafletMap.exposed function
  } catch (err) {
    console.warn('Failed to focus on zone after undo:', err);
  }

  $q.notify({
    type: 'info',
    message: 'Undid unsaved changes.',
    position: 'top'
  });
};
</script>