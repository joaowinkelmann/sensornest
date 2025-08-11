<template>
  <div class="bg-white rounded-lg shadow p-6 flex-grow flex flex-col">
    <h2 class="text-xl font-bold text-gray-800 mb-4">Zone Manager</h2>
    <div v-if="!meshesStore.selectedMeshId" class="text-center text-gray-500 flex-grow flex items-center justify-center">
      <p>Please select a mesh to manage its zones.</p>
    </div>
    <div v-else class="flex-grow flex flex-col md:flex-row gap-6 h-full min-h-[500px]">
      <!-- Zone List -->
      <div class="w-full md:w-1/3">
        <h3 class="font-semibold text-gray-700 mb-3">Zones in {{ selectedMeshName }}</h3>
        <ul v-if="zonesStore.zones.length > 0" class="space-y-2">
          <li v-for="zone in zonesStore.zones" :key="zone.id" class="bg-gray-50 p-3 rounded-md shadow-sm border border-gray-200">
            {{ zone.name }}
          </li>
        </ul>
        <p v-else class="text-sm text-gray-400 mt-4">
          No zones found. Draw one on the map to get started!
        </p>
      </div>
      <!-- Map -->
      <div class="w-full md:w-2/3 h-[400px] md:h-auto border rounded-lg overflow-hidden">
        <LeafletMap :zones="zonesStore.zones" @zone-created="handleZoneCreated" />
      </div>
    </div>
  </div>
</template>

<script setup>
import { watch, computed } from 'vue';
import { useMeshesStore } from '@/stores/meshes';
import { useZonesStore } from '@/stores/zones';
import LeafletMap from '@/components/LeafletMap.vue';

const meshesStore = useMeshesStore();
const zonesStore = useZonesStore();

// Watch for the selected mesh ID to change, then fetch the corresponding zones
watch(() => meshesStore.selectedMeshId, (newMeshId) => {
  zonesStore.fetchZones(newMeshId);
}, { immediate: true });

// A computed property to get the name of the currently selected mesh
const selectedMeshName = computed(() => {
  const mesh = meshesStore.meshes.find(m => m.id === meshesStore.selectedMeshId);
  return mesh ? mesh.name : '';
});

// Handler for when a new zone is drawn on the map
const handleZoneCreated = ({ name, vertices }) => {
  if (meshesStore.selectedMeshId) {
    zonesStore.createZone(meshesStore.selectedMeshId, name, vertices);
  }
};
</script>