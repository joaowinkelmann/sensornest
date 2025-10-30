<template>
  <div class="leaflet-map-wrapper">
    <div ref="mapContainer" class="leaflet-map-container"></div>

    <!-- Map Controls -->
    <MapControls
      :mode="mode"
      :active-zone="activeZone"
      @mode-changed="handleModeChange"
      @draw-zone-clicked="$emit('draw-zone-clicked')"
    />

    <div v-if="mode === 'draw-zone'" class="new-zone-controls">
      <q-btn
        push
        color="positive"
        icon="save"
        :label="t('leafletMap.saveZoneButton')"
        :disable="newZoneVertices.length < 3"
        @click="$emit('save-new-zone', newZoneVertices)"
      />
      <q-btn
        push
        color="negative"
        icon="cancel"
        :label="t('leafletMap.cancelButton')"
        @click="$emit('cancel-new-zone')"
      />
    </div>
  </div>
</template>

<script setup>
import { ref, onMounted, onUnmounted, watch } from 'vue';
import L from 'leaflet';
import MapControls from './MapControls.vue';
import { useMapManager } from '../composables/useMapManager';
import { useZoneRenderer } from '../composables/useZoneRenderer';
import { useDeviceRenderer } from '../composables/useDeviceRenderer';
import { useI18n } from 'vue-i18n';

const { t } = useI18n();
// Define props
const props = defineProps({
  mesh: {
    type: Object,
    default: null
  },
  zones: {
    type: Array,
    required: true
  },
  activeZoneId: {
    type: [String, Number],
    default: null
  },
  activeZone: {
    type: Object,
    default: null
  },
  centerLat: {
    type: Number,
    default: -23.5505
  },
  centerLng: {
    type: Number,
    default: -46.6333
  },
  devices: {
    type: Array,
    default: () => []
  },
  conversionFn: {
    type: Function,
    required: true
  },
  editMode: {
    type: String,
    default: 'zones'
  }
});

// Define emits
const emit = defineEmits([
  'vertex-added',
  'vertex-removed',
  'vertex-moved',
  'device-moved',
  'zone-clicked',
  'draw-zone-clicked',
  'save-new-zone',
  'cancel-new-zone'
]);

// Reactive references
const mapContainer = ref(null);
const mode = ref(null);
const newZoneVertices = ref([]);
const newZoneLayer = ref(null);

// Use composables for map management
const {
  map,
  initializeMap,
  destroyMap,
} = useMapManager(mapContainer);

const {
  drawZones,
  clearZoneLayers
} = useZoneRenderer(map, props, emit);

const {
  drawDevices,
  clearDeviceMarkers
} = useDeviceRenderer(map, props, emit);

// Mode management
const handleModeChange = (newMode) => {
  mode.value = newMode;
  // setMapDragging(newMode === null); // REMOVED: keep map movable in all modes

  // Redraw zones to show/hide vertex markers based on mode
  drawZones(props.zones, props.activeZoneId, mode.value);

  // Show/hide devices based on mode
  if (newMode && newMode !== 'move-devices') {
    clearDeviceMarkers();
  } else {
    drawDevices(props.devices, props.conversionFn, newMode);
  }
};

// Map click handler
const handleMapClick = (event) => {
  if (mode.value === 'add' && props.activeZoneId) {
    emit('vertex-added', event.latlng);
  } else if (mode.value === 'draw-zone') {
    newZoneVertices.value.push(event.latlng);
    drawNewZoneLayer();
  }
};

const drawNewZoneLayer = () => {
  if (newZoneLayer.value) {
    map.value.removeLayer(newZoneLayer.value);
  }
  if (newZoneVertices.value.length > 0) {
    newZoneLayer.value = L.polygon(newZoneVertices.value, {
      color: '#4CAF50',
      weight: 3,
      opacity: 0.7,
      fillColor: '#4CAF50',
      fillOpacity: 0.3
    }).addTo(map.value);
  }
};

// Helper to refresh device positions (re-draw markers)
const refreshDevicePositions = () => {
  // Only show devices when not in an edit mode
  if (mode.value === null) {
    drawDevices(props.devices, props.conversionFn, mode.value);
  }
};

const setupMapEvents = () => {
  if (map.value) {
    map.value.on('click', handleMapClick);
    map.value.on('zoomend', refreshDevicePositions);
    map.value.on('moveend', refreshDevicePositions);
  }
};

// Lifecycle hooks
onMounted(async () => {
  await initializeMap({
    lat: props.centerLat,
    lon: props.centerLng,
    mesh: props.mesh
  });
  setupMapEvents();
  drawZones(props.zones, props.activeZoneId, mode.value);
  drawDevices(props.devices, props.conversionFn, mode.value);
});

onUnmounted(() => {
  // Clean up listeners
  if (map.value) {
    map.value.off('zoomend', refreshDevicePositions);
    map.value.off('moveend', refreshDevicePositions);
    map.value.off('click', handleMapClick);
  }
  clearZoneLayers();
  clearDeviceMarkers();
  destroyMap();
});

// Watchers
watch(() => mode.value, (newMode, oldMode) => {
  // setMapDragging(newMode === null); // REMOVED: keep map movable in all modes
  drawZones(props.zones, props.activeZoneId, newMode);
  if (newMode && newMode !== 'move-devices') {
    clearDeviceMarkers();
  } else {
    // This will draw devices for 'move-devices' mode and when mode is null
    drawDevices(props.devices, props.conversionFn, newMode);
  }

  if (oldMode === 'draw-zone' && newMode !== 'draw-zone') {
    newZoneVertices.value = [];
    if (newZoneLayer.value) {
      map.value.removeLayer(newZoneLayer.value);
      newZoneLayer.value = null;
    }
  }
});

watch(() => [props.zones, props.activeZoneId], () => {
  drawZones(props.zones, props.activeZoneId, mode.value);
}, { deep: true });

watch(() => props.devices, (newDevices) => {
  // Only redraw devices if we are not in a vertex editing mode
  if (mode.value === null || mode.value === 'move-devices') {
    drawDevices(newDevices, props.conversionFn, mode.value);
  }
}, { deep: true });

watch(() => props.editMode, () => {
  drawDevices(props.devices, props.conversionFn, mode.value);
});

watch(() => props.mesh, async (newMesh) => {
  await initializeMap({
    lat: props.centerLat,
    lon: props.centerLng,
    mesh: newMesh,
  });
  // After re-initializing, we need to redraw everything and re-attach events
  setupMapEvents();
  drawZones(props.zones, props.activeZoneId, mode.value);
  drawDevices(props.devices, props.conversionFn, mode.value);
}, { deep: true });

// Expose methods for parent component
defineExpose({
  setMode: (newMode) => {
    mode.value = newMode;
  },
  getCurrentMode: () => mode.value
});
</script>

<style scoped>
.leaflet-map-wrapper {
  position: relative;
  width: 100%;
  height: 100%; /* fill parent, not viewport */
  background: #f0f0f0;
}

.new-zone-controls {
  position: absolute;
  top: 20px;
  left: 50%;
  transform: translateX(-50%);
  z-index: 1001;
  display: flex;
  gap: 12px;
  background: rgba(255, 255, 255, 0.8);
  padding: 12px;
  border-radius: 8px;
  box-shadow: 0 2px 8px rgba(0,0,0,0.15);
}

.leaflet-map-container {
  height: 100%; /* fill wrapper, not viewport */
  width: 100%;
  border-radius: 0;
}

/* Loading state */
.leaflet-map-container:empty::before {
  content: 'Loading map...';
  position: absolute;
  top: 50%;
  left: 50%;
  transform: translate(-50%, -50%);
  color: #666;
  font-size: 16px;
  z-index: 1000;
}

/* Mobile adjustments */
@media (max-width: 768px) {
  .leaflet-map-wrapper {
    height: 100%; /* keep consistent */
  }
}

/* Dark mode */
@media (prefers-color-scheme: dark) {
  .leaflet-map-wrapper {
    background: #1a1a1a;
  }
}
</style>