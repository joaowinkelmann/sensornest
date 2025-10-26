<template>
  <q-dialog :model-value="show" @update:model-value="$emit('close')" persistent>
    <q-card style="width: 700px; max-width: 80vw;">
      <q-card-section>
        <div class="text-h6">{{ isEditing ? 'Edit Mesh' : 'Create New Mesh' }}</div>
      </q-card-section>

      <q-card-section class="q-pt-none">
        <q-input v-model="meshData.name" label="Mesh Name" autofocus :rules="[val => !!val || 'Name is required']" />

        <q-tabs v-model="tab" class="text-grey" active-color="primary" indicator-color="primary" align="justify"
          narrow-indicator>
          <q-tab name="osm" label="OpenStreetMap" />
          <q-tab name="custom" label="Custom Map" />
        </q-tabs>

        <q-separator />

        <q-tab-panels v-model="tab" animated>
          <q-tab-panel name="osm">
            <div class="q-my-md">
              <q-btn @click="getUserLocation" label="Get Current Location" icon="my_location" color="secondary" />
            </div>
            <div ref="mapContainer" style="height: 300px; width: 100%;"></div>
            <div class="row q-col-gutter-sm q-mt-sm">
              <div class="col">
                <q-input v-model.number="meshData.lat" type="number" label="Latitude" />
              </div>
              <div class="col">
                <q-input v-model.number="meshData.lon" type="number" label="Longitude" />
              </div>
            </div>
          </q-tab-panel>

          <q-tab-panel name="custom">
            <q-file v-model="customMapFile" label="Upload Custom Map Image" accept="image/*" class="q-mt-md">
              <template v-slot:prepend>
                <q-icon name="attach_file" />
              </template>
            </q-file>
            <div v-if="meshData.mapUrl" class="q-mt-sm">
              <p>Current map URL: <code>{{ meshData.mapUrl }}</code></p>
            </div>
          </q-tab-panel>
        </q-tab-panels>
      </q-card-section>

      <q-card-actions align="right">
        <q-btn flat label="Cancel" color="primary" @click="$emit('close')" />
        <q-btn :label="isEditing ? 'Save Changes' : 'Create'" color="primary" @click="handleSubmit" />
      </q-card-actions>
    </q-card>
  </q-dialog>
</template>

<script setup>
import { ref, watch, onMounted, nextTick } from 'vue';
import { useQuasar } from 'quasar';
import L from 'leaflet';
import { useMeshesStore } from '@/stores/meshes';
import 'leaflet/dist/leaflet.css';

const props = defineProps({
  show: Boolean,
  meshToEdit: Object,
});

const emit = defineEmits(['close', 'mesh-created', 'mesh-updated']);

const $q = useQuasar();
const meshesStore = useMeshesStore();

const isEditing = ref(false);
const tab = ref('osm');
const meshData = ref({
  name: '',
  lat: -23.5505,
  lon: -46.6333,
  mapUrl: null,
});
const customMapFile = ref(null);
const mapContainer = ref(null);
let map = null;
let marker = null;

watch(() => props.show, (newVal) => {
  if (newVal) {
    isEditing.value = !!props.meshToEdit;
    if (isEditing.value) {
      meshData.value = { ...props.meshToEdit };
      tab.value = props.meshToEdit.mapUrl ? 'custom' : 'osm';
    } else {
      meshData.value = { name: '', lat: -23.5505, lon: -46.6333, mapUrl: null };
      tab.value = 'osm';
    }
    customMapFile.value = null;

    if (tab.value === 'osm') {
      nextTick(initMap);
    }
  } else {
    destroyMap();
  }
});

watch(tab, (newTab) => {
  if (newTab === 'osm' && props.show) {
    nextTick(initMap);
  } else {
    destroyMap();
  }
});

watch(() => [meshData.value.lat, meshData.value.lon], () => {
  if (map && marker) {
    const newLatLng = L.latLng(meshData.value.lat, meshData.value.lon);
    map.setView(newLatLng);
    marker.setLatLng(newLatLng);
  }
});


const initMap = () => {
  if (mapContainer.value && !map) {
    map = L.map(mapContainer.value).setView([meshData.value.lat, meshData.value.lon], 13);
    L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
      maxZoom: 19,
    }).addTo(map);

    marker = L.marker([meshData.value.lat, meshData.value.lon], { draggable: true }).addTo(map);

    marker.on('dragend', (event) => {
      const position = event.target.getLatLng();
      meshData.value.lat = position.lat;
      meshData.value.lon = position.lng;
    });

    map.on('click', (event) => {
      const position = event.latlng;
      meshData.value.lat = position.lat;
      meshData.value.lon = position.lng;
      marker.setLatLng(position);
    });
  }
};

const destroyMap = () => {
  if (map) {
    map.remove();
    map = null;
    marker = null;
  }
};

const getUserLocation = () => {
  if (navigator.geolocation) {
    navigator.geolocation.getCurrentPosition(
      (position) => {
        meshData.value.lat = position.coords.latitude;
        meshData.value.lon = position.coords.longitude;
      },
      (error) => {
        console.log(error, "Error getting location");
        $q.notify({ type: 'negative', message: 'Could not get your location.' });
      }
    );
  } else {
    $q.notify({ type: 'negative', message: 'Geolocation is not supported by your browser.' });
  }
};

const handleSubmit = async () => {
  if (!meshData.value.name) {
    $q.notify({ type: 'negative', message: 'Please enter a name for the mesh.' });
    return;
  }

  try {
    if (isEditing.value) {
      const meshId = meshData.value.id;
      // If a new map file is provided, upload it. The store will be updated automatically.
      if (tab.value === 'custom' && customMapFile.value) {
        await meshesStore.uploadMapImage(customMapFile.value, meshId);
        // The store is now updated, so we can just close.
        emit('close');
        return;
      }
      // Otherwise, just update the metadata.
      const {...updatePayload } = { ...meshData.value };
      await meshesStore.updateMesh(meshId, updatePayload);
      emit('close');

    } else {
      // Create a new mesh
      const createPayload = {
        name: meshData.value.name,
        lat: tab.value === 'osm' ? meshData.value.lat : null,
        lon: tab.value === 'osm' ? meshData.value.lon : null,
        mapUrl: null // Will be set by upload if applicable
      };
      const newMesh = await meshesStore.createMesh(createPayload);

      // If a custom map is provided, upload it for the new mesh
      if (tab.value === 'custom' && customMapFile.value) {
        await meshesStore.uploadMapImage(customMapFile.value, newMesh.id);
      }

      // Select the new mesh and close the dialog
      meshesStore.selectMesh(newMesh.id);
      emit('close');
    }
  } catch (error) {
    console.error('Mesh save/upload failed:', error);
    $q.notify({ type: 'negative', message: 'An error occurred while saving the mesh.' });
  }
};

onMounted(() => {
  if (props.show) {
    //
  }
});

</script>