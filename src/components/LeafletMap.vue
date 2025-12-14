<template>
  <div class="leaflet-map-wrapper">
    <div ref="mapContainer" class="leaflet-map-container"></div>

    <!-- Controles do Mapa -->
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
// Definição de props
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

// Definição de emits
const emit = defineEmits([
  'vertex-added',
  'vertex-removed',
  'vertex-moved',
  'device-moved',
  'zone-clicked',
  'draw-zone-clicked',
  'save-new-zone',
  'cancel-new-zone',
  'device-clicked',
  'device-zone-clicked'
]);

// Referências reativas
const mapContainer = ref(null);
const mode = ref(null);
const newZoneVertices = ref([]);
const newZoneLayer = ref(null);

// Usa composables para gerenciamento do mapa
const {
  map,
  initializeMap,
  destroyMap,
} = useMapManager(mapContainer);

const {
  drawZones,
  clearZoneLayers,
  enableEditMode,
  clearVertexMarkers
} = useZoneRenderer(map, props, emit);

const {
  drawDevices,
  clearDeviceMarkers
} = useDeviceRenderer(map, props, emit);

// Gerenciamento de modo
const handleModeChange = (newMode) => {
  mode.value = newMode;
  
  // Limpa marcadores antigos primeiro
  clearVertexMarkers();

  // Redesenha zonas (renderização pura)
  drawZones(props.zones, props.activeZoneId);

  // Aplica modo de edição (interação)
  enableEditMode(props.activeZoneId, newMode);

  // Mostra/oculta dispositivos baseado no modo
  if (newMode && newMode !== 'move-devices') {
    clearDeviceMarkers();
  } else {
    drawDevices(props.devices, props.conversionFn, newMode);
  }
};

// Handler de clique no mapa
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



const setupMapEvents = () => {
  if (map.value) {
    map.value.on('click', handleMapClick);
  }
};

// Hooks de ciclo de vida
onMounted(async () => {
  await initializeMap({
    lat: props.centerLat,
    lon: props.centerLng,
    mesh: props.mesh
  });
  setupMapEvents();
  drawZones(props.zones, props.activeZoneId);
  enableEditMode(props.activeZoneId, mode.value);
  drawDevices(props.devices, props.conversionFn, mode.value);
});

onUnmounted(() => {
  if (map.value) {
    map.value.off('click', handleMapClick);
  }
  clearZoneLayers();
  clearDeviceMarkers();
  destroyMap();
});

// Observadores (Watchers)
watch(() => mode.value, (newMode, oldMode) => {
  handleModeChange(newMode);

  if (oldMode === 'draw-zone' && newMode !== 'draw-zone') {
    newZoneVertices.value = [];
    if (newZoneLayer.value) {
      map.value.removeLayer(newZoneLayer.value);
      newZoneLayer.value = null;
    }
  }
});

watch(() => [props.zones, props.activeZoneId], () => {
  drawZones(props.zones, props.activeZoneId);
  enableEditMode(props.activeZoneId, mode.value);
}, { deep: true });

watch(() => props.devices, (newDevices) => {
  // Só redesenha dispositivos se não estivermos em modo de edição de vértices
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
  // Após reinicializar, precisamos redesenhar tudo e reanexar eventos
  setupMapEvents();
  drawZones(props.zones, props.activeZoneId);
  enableEditMode(props.activeZoneId, mode.value);
  drawDevices(props.devices, props.conversionFn, mode.value);
}, { deep: true });

// Expõe métodos para o componente pai
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
  height: 100%; /* preenche o pai, não a viewport */
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
  height: 100%; /* preenche o pai, não a viewport */
  width: 100%;
  border-radius: 0;
}

/* Estado de carregamento */
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

/* Ajustes para mobile */
@media (max-width: 768px) {
  .leaflet-map-wrapper {
    height: 100%; /* keep consistent */
  }
}

/* Modo escuro */
@media (prefers-color-scheme: dark) {
  .leaflet-map-wrapper {
    background: #1a1a1a;
  }
}
</style>