<template>
  <div class="leaflet-map-wrapper">
    <div ref="mapContainer" class="leaflet-map-container"></div>
    <div class="leaflet-controls-container">
      <q-btn-group push>
        <q-btn push label="Add" icon="add_circle" @click="setMode('add')" :color="mode === 'add' ? 'primary' : 'white'" :text-color="mode === 'add' ? 'white' : 'black'" />
        <q-btn push label="Remove" icon="remove_circle" @click="setMode('remove')" :color="mode === 'remove' ? 'negative' : 'white'" :text-color="mode === 'remove' ? 'white' : 'black'" />
        <q-btn push label="Move" icon="open_with" @click="setMode('move')" :color="mode === 'move' ? 'info' : 'white'" :text-color="mode === 'move' ? 'white' : 'black'" />
        <q-btn push label="Normal" icon="pan_tool" @click="setMode(null)" :color="!mode ? 'positive' : 'white'" :text-color="!mode ? 'white' : 'black'" />
      </q-btn-group>
      <q-btn
        push
        label="Save Zone"
        icon="save"
        color="amber"
        class="q-ml-md"
        :disabled="!activeZone"
        @click="$emit('zone-updated')"
      />
    </div>
  </div>
</template>

<script setup>
import { ref, onMounted, onUnmounted, watch } from 'vue';
import L from 'leaflet';

// Define props to receive data from the parent component
const props = defineProps({
  zones: { type: Array, required: true },
  activeZoneId: { type: String, default: null },
  activeZone: { type: Object, default: null },
});

// Define the events that this component can emit to its parent
const emit = defineEmits(['vertex-added', 'vertex-removed', 'vertex-moved', 'zone-updated']);

const mapContainer = ref(null);
const mode = ref(null); // 'add', 'remove', 'move', or null
let map = null;
let zoneLayers = {};

const setMode = (newMode) => {
  mode.value = newMode;
};

// This function enables or disables map dragging and other interactions
const setMapDragging = (enabled) => {
  if (!map) return;
  if (enabled) {
    map.dragging.enable();
    map.touchZoom.enable();
    map.doubleClickZoom.enable();
    map.scrollWheelZoom.enable();
  } else {
    map.dragging.disable();
    map.touchZoom.disable();
    map.doubleClickZoom.disable();
    map.scrollWheelZoom.disable();
  }
};

// This function draws/updates the zone polygons on the map
const drawZones = (zones) => {
  if (!map) return;

  const existingZoneIds = Object.keys(zoneLayers);
  const newZoneIds = zones.map(z => z.id);

  // Remove layers for zones that no longer exist
  existingZoneIds.forEach(zoneId => {
    if (!newZoneIds.includes(zoneId)) {
      if (zoneLayers[zoneId]) {
        if (zoneLayers[zoneId]._vertexMarkers) {
          zoneLayers[zoneId]._vertexMarkers.forEach(marker => map.removeLayer(marker));
        }
        map.removeLayer(zoneLayers[zoneId]);
      }
      delete zoneLayers[zoneId];
    }
  });

  if (zones && zones.length > 0) {
    zones.forEach(zone => {
      const isActive = zone.id === props.activeZoneId;
      const style = {
        color: isActive ? '#d97706' : '#3388ff', // Amber for active, blue for others
        weight: isActive ? 3 : 2,
        opacity: 1,
        fillOpacity: 0.2,
      };

      if (Array.isArray(zone.vertices) && zone.vertices.length > 0) {
        const latlngs = zone.vertices.map(v => [v.lat, v.lng]);
        
        if (zoneLayers[zone.id]) {
          zoneLayers[zone.id].setLatLngs(latlngs).setStyle(style);
        } else {
          const polygon = L.polygon(latlngs, style).bindPopup(`<b>${zone.name}</b>`).addTo(map);
          zoneLayers[zone.id] = polygon;
        }

        // Manage vertex markers
        if (zoneLayers[zone.id]._vertexMarkers) {
          zoneLayers[zone.id]._vertexMarkers.forEach(marker => map.removeLayer(marker));
        }
        zoneLayers[zone.id]._vertexMarkers = [];

        if (isActive && (mode.value === 'remove' || mode.value === 'move')) {
          zone.vertices.forEach((v, idx) => {
            const marker = L.marker([v.lat, v.lng], {
              draggable: mode.value === 'move',
              icon: L.divIcon({
                className: `vertex-marker vertex-${mode.value}-marker`,
                html: `<span>${mode.value.charAt(0).toUpperCase()}</span>`,
                iconSize: [24, 24],
                iconAnchor: [12, 12],
              }),
            });

            if (mode.value === 'remove') {
              marker.on('click', () => emit('vertex-removed', idx));
            } else if (mode.value === 'move') {
              marker.on('dragstart', () => setMapDragging(false));
              marker.on('dragend', (e) => {
                setMapDragging(true);
                emit('vertex-moved', { index: idx, latlng: e.target.getLatLng() });
              });
            }
            marker.addTo(map);
            zoneLayers[zone.id]._vertexMarkers.push(marker);
          });
        }
      }
    });

    const allLayers = Object.values(zoneLayers).filter(l => l instanceof L.Polygon && l.getLatLngs()[0].length > 0);
    if (allLayers.length > 0) {
      const group = new L.featureGroup(allLayers);
      map.fitBounds(group.getBounds().pad(0.1));
    }
  }
};

onMounted(() => {
  if (mapContainer.value) {
    map = L.map(mapContainer.value).setView([-23.5505, -46.6333], 13);
    L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
      attribution: '&copy; <a href="https://www.openstreetmap.org/copyright">OpenStreetMap</a> contributors',
    }).addTo(map);

    map.on('click', (event) => {
      if (props.activeZoneId && mode.value === 'add') {
        emit('vertex-added', event.latlng);
      }
    });

    drawZones(props.zones);
  }
});

onUnmounted(() => {
  if (map) {
    map.remove();
    map = null;
  }
});

watch(() => mode.value, (newMode) => {
  setMapDragging(newMode !== 'move');
  drawZones(props.zones); // Redraw to show/hide markers
});

watch(() => [props.zones, props.activeZoneId], () => {
  drawZones(props.zones);
}, { deep: true });
</script>

<style>
.leaflet-map-wrapper {
  position: relative;
  width: 100%;
  height: 100%;
  min-height: 600px;
}
.leaflet-map-container {
  width: 100%;
  height: 100%;
}
.leaflet-controls-container {
  position: absolute;
  top: 10px;
  left: 50%;
  transform: translateX(-50%);
  z-index: 1000; /* Above the map */
  background-color: rgba(255, 255, 255, 0.8);
  padding: 8px;
  border-radius: 8px;
  box-shadow: 0 2px 8px rgba(0,0,0,0.3);
}
.vertex-marker {
  cursor: pointer;
  background: white;
  border-radius: 50%;
  display: flex;
  align-items: center;
  justify-content: center;
  width: 24px;
  height: 24px;
  font-weight: bold;
  font-family: sans-serif;
}
.vertex-remove-marker {
  border: 2px solid #C10015; /* Quasar negative */
  color: #C10015;
}
.vertex-move-marker {
  cursor: grab;
  border: 2px solid #31CCEC; /* Quasar info */
  color: #31CCEC;
}
</style>