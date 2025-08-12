<template>
  <div class="flex flex-col h-screen bg-gray-100">
    <!-- Header -->
    <header class="bg-white shadow-sm p-4 flex justify-between items-center z-10">
      <h1 class="text-2xl font-bold text-gray-800">TempMesh Dashboard</h1>
      <div v-if="authStore.user" class="flex items-center">
        <span class="text-gray-600 mr-4">Welcome, {{ authStore.user.name }}</span>
        <button @click="authStore.logout()" class="bg-red-500 hover:bg-red-600 text-white font-bold py-2 px-4 rounded-md transition-colors">
          Logout
        </button>
      </div>
    </header>

    <!-- Main Content -->
    <main class="flex-grow p-4 sm:p-6 lg:p-8 overflow-y-auto">
      <div class="max-w-7xl mx-auto flex flex-col md:flex-row gap-8">
        <!-- Left Column: Controls -->
        <div class="w-full md:w-1/3 flex flex-col gap-6">
          <MeshManager />
          
          <!-- Zone List -->
          <div class="bg-white rounded-lg shadow p-4">
            <h3 class="font-semibold text-gray-700 mb-3">Zones</h3>
            <ul v-if="zonesStore.zones.length > 0" class="space-y-2">
              <li 
                v-for="zone in zonesStore.zones" 
                :key="zone.id" 
                @click="zonesStore.setActiveZone(zone.id)"
                class="p-3 rounded-md shadow-sm border cursor-pointer transition-colors"
                :class="{
                  'bg-indigo-100 border-indigo-400 ring-2 ring-indigo-300': zonesStore.activeZoneId === zone.id,
                  'bg-gray-50 border-gray-200 hover:bg-gray-100': zonesStore.activeZoneId !== zone.id
                }"
              >
                {{ zone.name }}
              </li>
            </ul>
            <p v-else class="text-sm text-gray-400 mt-4">
              No zones found for this mesh.
            </p>
          </div>
        </div>

        <!-- Right Column: Map -->
        <div class="w-full md:w-2/3 bg-white rounded-lg shadow overflow-hidden flex flex-col">
          <!-- Toolbar -->
          <div class="flex items-center gap-2 bg-gray-50 border-b px-4 py-2">
            <button
              @click="setMode('add')"
              :class="[
                'px-3 py-1 rounded font-bold',
                mode === 'add' ? 'bg-indigo-600 text-white' : 'bg-gray-200 text-gray-700'
              ]"
              title="Add Vertices"
            >
              A
            </button>
            <button
              @click="setMode('remove')"
              :class="[
                'px-3 py-1 rounded font-bold',
                mode === 'remove' ? 'bg-red-600 text-white' : 'bg-gray-200 text-gray-700'
              ]"
              title="Remove Vertices"
            >
              R
            </button>
            <button
              @click="setMode('move')"
              :class="[
                'px-3 py-1 rounded font-bold',
                mode === 'move' ? 'bg-blue-600 text-white' : 'bg-gray-200 text-gray-700'
              ]"
              title="Move Vertices"
            >
              M
            </button>
              <button
                @click="setMode(null)"
                :class="[
                  'px-3 py-1 rounded font-bold',
                  mode === null ? 'bg-green-600 text-white' : 'bg-gray-200 text-gray-700'
                ]"
                title="Normal Mode"
              >
                N
              </button>
              <button
                @click="saveZone"
                :disabled="!zonesStore.activeZone"
                class="px-3 py-1 rounded font-bold bg-yellow-500 text-white disabled:opacity-50"
                title="Save Zone"
              >
                S
              </button>
              <span class="ml-4 text-xs text-gray-500">
                Mode: <span class="font-bold">{{ modeLabel }}</span>
              </span>
            </div>
          <div class="flex-grow min-h-[500px]">
            <LeafletMap 
              :zones="convertedZones" 
              :active-zone-id="zonesStore.activeZoneId"
              :mode="mode"
              @vertex-added="handleVertexAdded"
              @vertex-removed="handleVertexRemoved"
              @vertex-moved="handleVertexMoved"
            />
          </div>
        </div>
      </div>
    </main>
  </div>
</template>

<script setup>
import { computed, ref } from 'vue';
import { useAuthStore } from '@/stores/auth';
import { useMeshesStore } from '@/stores/meshes';
import { useZonesStore } from '@/stores/zones';
import MeshManager from '@/components/MeshManager.vue';
import LeafletMap from '@/components/LeafletMap.vue';

const authStore = useAuthStore();
const meshesStore = useMeshesStore();
const zonesStore = useZonesStore();

const BASE_LAT = -23.5505 // São Paulo latitude
const BASE_LNG = -46.6333 // São Paulo longitude
const SCALE = 0.0001 // Adjust scale for reasonable size on map

// Convert API vertices {x, y} to Leaflet {lat, lng}
const convertedZones = computed(() =>
  zonesStore.zones.map(zone => {
    if (!zone.vertices || zone.vertices.length === 0) {
      return { ...zone, vertices: [] };
    }
    // Find the centroid of the zone in local coordinates
    const xs = zone.vertices.map(v => v.x);
    const ys = zone.vertices.map(v => v.y);
    const centroidX = xs.reduce((a, b) => a + b, 0) / xs.length;
    const centroidY = ys.reduce((a, b) => a + b, 0) / ys.length;

    return {
      ...zone,
      vertices: zone.vertices.map(v => ({
        lat: BASE_LAT + (v.y - centroidY) * SCALE,
        lng: BASE_LNG + (v.x - centroidX) * SCALE,
      })),
    };
  })
);

// Convert a Leaflet {lat, lng} point back to API {x, y}
const convertToApiVertex = (latlng) => {
  const activeZone = zonesStore.activeZone;
  if (!activeZone || activeZone.vertices.length === 0) return null;

  const xs = activeZone.vertices.map(v => v.x);
  const ys = activeZone.vertices.map(v => v.y);
  const centroidX = xs.reduce((a, b) => a + b, 0) / xs.length;
  const centroidY = ys.reduce((a, b) => a + b, 0) / ys.length;

  return {
    x: Math.round(((latlng.lng - BASE_LNG) / SCALE) + centroidX),
    y: Math.round(((latlng.lat - BASE_LAT) / SCALE) + centroidY),
  };
};

const mode = ref(null); // null, 'add', 'remove'

const setMode = (newMode) => {
  mode.value = newMode;
};

const modeLabel = computed(() => {
  if (mode.value === 'add') return 'Add Vertices';
  if (mode.value === 'remove') return 'Remove Vertices';
  if (mode.value === 'move') return 'Move Vertices';
  return 'Normal';
});

const handleVertexAdded = (latlng) => {
  if (mode.value !== 'add') return;

  const activeZone = zonesStore.activeZone;
  if (!activeZone) return;

  const newApiVertex = convertToApiVertex(latlng);
  if (!newApiVertex) return;

  const updatedVertices = [...activeZone.vertices, newApiVertex];
  const updatedZone = { ...activeZone, vertices: updatedVertices };
  
  // Optimistically update the local state
  const index = zonesStore.zones.findIndex(z => z.id === activeZone.id);
  if (index !== -1) {
    zonesStore.zones[index] = updatedZone;
  }

  // Here you would typically call the update action
  // zonesStore.updateZone(activeZone.id, { vertices: updatedVertices });
};

const handleVertexRemoved = (vertexIndex) => {
  if (mode.value !== 'remove') return;
  const activeZone = zonesStore.activeZone;
  if (!activeZone) return;
  const updatedVertices = activeZone.vertices.filter((_, idx) => idx !== vertexIndex);
  const updatedZone = { ...activeZone, vertices: updatedVertices };
  const index = zonesStore.zones.findIndex(z => z.id === activeZone.id);
  if (index !== -1) {
    zonesStore.zones[index] = updatedZone;
  }
  // zonesStore.updateZone(activeZone.id, { vertices: updatedVertices });
};

const handleVertexMoved = ({ index, latlng }) => {
  if (mode.value !== 'move') return;
  const activeZone = zonesStore.activeZone;
  if (!activeZone) return;
  const updatedVertices = activeZone.vertices.map((v, idx) =>
    idx === index
      ? convertToApiVertex(latlng)
      : v
  );
  const updatedZone = { ...activeZone, vertices: updatedVertices };
  const zoneIdx = zonesStore.zones.findIndex(z => z.id === activeZone.id);
  if (zoneIdx !== -1) {
    zonesStore.zones[zoneIdx] = updatedZone;
  }
  // zonesStore.updateZone(activeZone.id, { vertices: updatedVertices });
};
const saveZone = async () => {
  const zone = zonesStore.activeZone;
  if (!zone) return;
  try {
    // PATCH /zones/:id
    await zonesStore.updateZone(zone.id, {
      name: zone.name,
      vertices: zone.vertices,
      meshId: zone.meshId,
    });
    // Optionally show a success message
    alert('Zone saved!');
  } catch (e) {
    alert('Failed to save zone.');
  }
};
</script>