<template>
  <q-layout view="hHh LpR fFf">
    <!-- Header -->
    <q-header elevated class="bg-primary text-white">
      <q-toolbar>
        <q-toolbar-title>
          <q-avatar>
            <img src="https://cdn.quasar.dev/logo-v2/svg/logo-mono-white.svg">
          </q-avatar>
          TempMesh Dashboard
        </q-toolbar-title>
        <q-btn
          v-if="authStore.isAuthenticated"
          @click="authStore.logout()"
          flat
          round
          dense
          icon="logout"
          label="Logout"
        />
      </q-toolbar>
    </q-header>

    <q-page-container>
      <q-page padding v-if="authStore.isAuthenticated">
        <div class="row q-col-gutter-md">
          <!-- Left Column: Controls -->
          <div class="col-12 col-md-4">
            <div class="q-gutter-y-md">
              <MeshManager />
              
              <!-- Zone List -->
              <q-card flat bordered>
                <q-card-section>
                  <div class="text-h6">Zones</div>
                </q-card-section>
                <q-list separator v-if="zonesStore.zones.length > 0">
                  <q-item 
                    v-for="zone in zonesStore.zones" 
                    :key="zone.id" 
                    clickable 
                    v-ripple
                    @click="zonesStore.setActiveZone(zone.id)"
                    :active="zonesStore.activeZoneId === zone.id"
                    active-class="bg-teal-1 text-grey-8"
                  >
                    <q-item-section>{{ zone.name }}</q-item-section>
                  </q-item>
                </q-list>
                <q-card-section v-else class="text-grey-6">
                  No zones found for this mesh.
                </q-card-section>
              </q-card>
            </div>
          </div>

          <!-- Right Column: Map -->
          <div class="col-12 col-md-8">
            <q-card flat bordered class="full-height">
              <LeafletMap 
                :zones="convertedZones" 
                :active-zone-id="zonesStore.activeZoneId"
                :active-zone="zonesStore.activeZone"
                @vertex-added="handleVertexAdded"
                @vertex-removed="handleVertexRemoved"
                @vertex-moved="handleVertexMoved"
                @zone-updated="saveZone"
              />
            </q-card>
          </div>
        </div>
      </q-page>
      
      <!-- Auth Modals -->
      <div v-else>
        <LoginModal 
          :show="!authStore.isAuthenticated && !showRegisterModal" 
          @show-register="showRegisterModal = true" 
        />
        <RegisterModal 
          :show="showRegisterModal" 
          @show-login="showRegisterModal = false" 
        />
      </div>
    </q-page-container>
  </q-layout>
</template>

<script setup>
import { computed, ref } from 'vue';
import { useAuthStore } from '@/stores/auth';
import { useMeshesStore } from '@/stores/meshes';
import { useZonesStore } from '@/stores/zones';
import MeshManager from '@/components/MeshManager.vue';
import LeafletMap from '@/components/LeafletMap.vue';
import LoginModal from '@/components/LoginModal.vue';
import RegisterModal from '@/components/RegisterModal.vue';

const authStore = useAuthStore();
const meshesStore = useMeshesStore();
const zonesStore = useZonesStore();

const showRegisterModal = ref(false);

const BASE_LAT = -23.5505 // São Paulo latitude
const BASE_LNG = -46.6333 // São Paulo longitude
const SCALE = 0.0001 // Adjust scale for reasonable size on map

// Convert API vertices {x, y} to Leaflet {lat, lng}
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
        lat: BASE_LAT + (v.y - centroidY) * SCALE,
        lng: BASE_LNG + (v.x - centroidX) * SCALE,
      })),
    };
  })
);

// Convert a Leaflet {lat, lng} point back to API {x, y}
const convertToApiVertex = (latlng) => {
  const activeZone = zonesStore.activeZone;
  if (!activeZone || activeZone.vertices.length === 0) {
     // If there are no vertices, create a new reference point
    const centroidX = 0;
    const centroidY = 0;
     return {
      x: Math.round(((latlng.lng - BASE_LNG) / SCALE) + centroidX),
      y: Math.round(((latlng.lat - BASE_LAT) / SCALE) + centroidY),
    };
  }

  const xs = activeZone.vertices.map(v => v.x);
  const ys = activeZone.vertices.map(v => v.y);
  const centroidX = xs.reduce((a, b) => a + b, 0) / xs.length;
  const centroidY = ys.reduce((a, b) => a + b, 0) / ys.length;

  return {
    x: Math.round(((latlng.lng - BASE_LNG) / SCALE) + centroidX),
    y: Math.round(((latlng.lat - BASE_LAT) / SCALE) + centroidY),
  };
};

const handleVertexAdded = (latlng) => {
  const activeZone = zonesStore.activeZone;
  if (!activeZone) return;

  const newApiVertex = convertToApiVertex(latlng);
  if (!newApiVertex) return;

  const updatedVertices = [...activeZone.vertices, newApiVertex];
  
  zonesStore.updateLocalZone(activeZone.id, { vertices: updatedVertices });
};

const handleVertexRemoved = (vertexIndex) => {
  const activeZone = zonesStore.activeZone;
  if (!activeZone) return;
  const updatedVertices = activeZone.vertices.filter((_, idx) => idx !== vertexIndex);
  zonesStore.updateLocalZone(activeZone.id, { vertices: updatedVertices });
};

const handleVertexMoved = ({ index, latlng }) => {
  const activeZone = zonesStore.activeZone;
  if (!activeZone) return;
  const newApiVertex = convertToApiVertex(latlng);
  if (!newApiVertex) return;
  
  const updatedVertices = activeZone.vertices.map((v, idx) =>
    idx === index ? newApiVertex : v
  );
  zonesStore.updateLocalZone(activeZone.id, { vertices: updatedVertices });
};

const saveZone = async () => {
  const zone = zonesStore.activeZone;
  if (!zone) return;
  try {
    await zonesStore.updateZone(zone.id, {
      name: zone.name,
      vertices: zone.vertices,
      meshId: zone.meshId,
    });
    // You can use Quasar's Notify plugin for a better user experience
    alert('Zone saved!');
  } catch (e) {
    console.error('Failed to save zone:', e);
    alert('Failed to save zone.');
  }
};
</script>