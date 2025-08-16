<template>
  <div class="leaflet-map-wrapper">
    <div ref="mapContainer" class="leaflet-map-container"></div>

    <!-- Map Controls -->
    <MapControls :mode="mode" :active-zone="activeZone" @mode-changed="handleModeChange" />
  </div>
</template>

<script setup>
import { ref, onMounted, onUnmounted, watch } from 'vue';
import L from 'leaflet';
import MapControls from './MapControls.vue';
import { useMapManager } from './composables/useMapManager';
import { useZoneRenderer } from './composables/useZoneRenderer';
import { useDeviceRenderer } from './composables/useDeviceRenderer';

// Define props
const props = defineProps({
  zones: {
    type: Array,
    required: true
  },
  activeZoneId: {
    type: String,
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
  }
});

// Define emits
const emit = defineEmits([
  'vertex-added',
  'vertex-removed',
  'vertex-moved',
]);

// Reactive references
const mapContainer = ref(null);
const mode = ref(null);

// Use composables for map management
const {
  map,
  initializeMap,
  destroyMap,
  setMapDragging
} = useMapManager(mapContainer);

const {
  zoneLayers,
  drawZones,
  clearZoneLayers
} = useZoneRenderer(map, props, emit);

const {
  drawDevices,
  clearDeviceMarkers
} = useDeviceRenderer(map);

// Mode management
const handleModeChange = (newMode) => {
  mode.value = newMode;
  setMapDragging(newMode !== 'move');
  // Redraw zones to show/hide vertex markers based on mode
  drawZones(props.zones, props.activeZoneId, mode.value);

  // Clear devices when entering an edit mode
  if (newMode) {
    clearDeviceMarkers();
  } else {
    drawDevices(props.devices, props.conversionFn);
  }
};

// Map click handler
const handleMapClick = (event) => {
  if (props.activeZoneId && mode.value === 'add') {
    emit('vertex-added', event.latlng);
  }
};

// Lifecycle hooks
onMounted(async () => {
  await initializeMap(props.centerLat, props.centerLng);
  if (map.value) {
    map.value.on('click', handleMapClick);
    drawZones(props.zones, props.activeZoneId, mode.value);
    drawDevices(props.devices, props.conversionFn);
  }
});

onUnmounted(() => {
  clearZoneLayers();
  clearDeviceMarkers();
  destroyMap();
});

// Watchers
watch(() => mode.value, (newMode) => {
  setMapDragging(newMode !== 'move');
  drawZones(props.zones, props.activeZoneId, newMode);
  if (newMode) {
    clearDeviceMarkers();
  } else {
    drawDevices(props.devices, props.conversionFn);
  }
});

watch(() => [props.zones, props.activeZoneId], () => {
  drawZones(props.zones, props.activeZoneId, mode.value);
}, { deep: true });

watch(() => props.devices, (newDevices) => {
  if (mode.value === null) { // Only draw if not in edit mode
    drawDevices(newDevices, props.conversionFn);
  }
}, { deep: true });

watch(() => [props.centerLat, props.centerLng], (newCenter) => {
  if (map.value && newCenter[0] && newCenter[1]) {
    map.value.setView([newCenter[0], newCenter[1]], 15);
  }
});

// Expose methods for parent component
defineExpose({
  setMode: (newMode) => {
    mode.value = newMode;
  },
  getCurrentMode: () => mode.value,
  focusOnZone: (zoneId) => {
    const zone = props.zones.find(z => z.id === zoneId);
    if (zone && zone.vertices?.length > 0) {
      const bounds = L.latLngBounds(zone.vertices.map(v => [v.lat, v.lng]));
      map.value?.fitBounds(bounds.pad(0.2));
    }
  }
});
</script>

<style scoped>
.leaflet-map-wrapper {
  position: relative;
  width: 100%;
  height: 100%;
  background: #f0f0f0;
}

.leaflet-map-container {
  height: 100vh;
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

/* Responsive adjustments */
@media (max-width: 768px) {
  .leaflet-map-container {
    height: 100vh;
    height: 100dvh;
    /* Dynamic viewport height for mobile */
  }
}

/* Dark mode support */
@media (prefers-color-scheme: dark) {
  .leaflet-map-wrapper {
    background: #1a1a1a;
  }
}
</style>